/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "dispatchable.h"
#include "mir/input/event_builder.h"

#include <boost/throw_exception.hpp>
#include <chrono>
#include <X11/Xutil.h>
#include <linux/input.h>
#include <inttypes.h>

#define MIR_LOG_COMPONENT "x11-dispatchable"
#include "mir/log.h"

// Uncomment for verbose output with log_info.
//#define MIR_ON_X11_INPUT_VERBOSE

// Due to a bug in Unity when keyboard is grabbed,
// client cannot be resized. This helps in debugging.
#define GRAB_KBD

namespace mi = mir::input;
namespace mix = mi::X;
namespace md = mir::dispatch;

mix::XDispatchable::XDispatchable(
    std::shared_ptr<::Display> const& conn,
    int raw_fd)
    : x11_connection(conn),
      fd(raw_fd),
      sink(nullptr),
      builder(nullptr)
{
}

mir::Fd mix::XDispatchable::watch_fd() const
{
    return fd;
}

bool mix::XDispatchable::dispatch(md::FdEvents events)
{
    auto ret = true;

    if (events & (md::FdEvent::remote_closed | md::FdEvent::error))
        ret = false;

    if (events & md::FdEvent::readable)
    {
        // This code is based on :
        // https://tronche.com/gui/x/xlib/events/keyboard-pointer/keyboard-pointer.html
        XEvent xev;

        XNextEvent(x11_connection.get(), &xev);

        if (sink)
        {
            switch (xev.type)
            {
#ifdef GRAB_KBD
            case FocusIn:
            {
                auto const& xfiev = (XFocusInEvent&)xev;
                XGrabKeyboard(xfiev.display, xfiev.window, True, GrabModeAsync, GrabModeAsync, CurrentTime);
                break;
            }

            case FocusOut:
            {
                auto const& xfoev = (XFocusOutEvent&)xev;
                XUngrabKeyboard(xfoev.display, CurrentTime);
                break;
            }
#endif
            case KeyPress:
            case KeyRelease:
            {
                auto& xkev = (XKeyEvent&)xev;
                static const int STRMAX = 32;
                char str[STRMAX];
                KeySym keysym;

#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("X11 key event :"
                    " type=%s, serial=%u, send_event=%d, display=%p, window=%p,"
                    " root=%p, subwindow=%p, time=%d, x=%d, y=%d, x_root=%d,"
                    " y_root=%d, state=%0X, keycode=%d, same_screen=%d",
                    xkev.type == KeyPress ? "down" : "up", xkev.serial,
                    xkev.send_event, xkev.display, xkev.window, xkev.root,
                    xkev.subwindow, xkev.time, xkev.x, xkev.y, xkev.x_root,
                    xkev.y_root, xkev.state, xkev.keycode, xkev.same_screen);
                auto count =
#endif
                XLookupString(&xkev, str, STRMAX, &keysym, NULL);

                MirInputEventModifiers modifiers = mir_input_event_modifier_none;
                if (xkev.state & ShiftMask)
                    modifiers |= mir_input_event_modifier_shift;
                if (xkev.state & ControlMask)
                    modifiers |=  mir_input_event_modifier_ctrl;
                if (xkev.state & Mod1Mask)
                    modifiers |=  mir_input_event_modifier_alt;
                if (xkev.state & Mod4Mask)
                    modifiers |=  mir_input_event_modifier_meta;

                auto event_time =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::milliseconds{xkev.time});

#ifdef MIR_ON_X11_INPUT_VERBOSE
                for (int i=0; i<count; i++)
                    mir::log_info("buffer[%d]='%c'", i, str[i]);
                mir::log_info("Mir key event : "
                    "key_code=%d, scan_code=%d, modifiers=%0X, event_time=%" PRId64,
                    keysym, xkev.keycode-8, modifiers, event_time);
#endif
                sink->handle_input(
                    *builder->key_event(
                        event_time,
                        xkev.type == KeyPress ?
                            mir_keyboard_action_down :
                            mir_keyboard_action_up,
                        keysym,
                        xkev.keycode-8,
                        modifiers
                    )
                );
                break;
            }

            case ButtonPress:
            case ButtonRelease:
            {
                auto const& xbev = (XButtonEvent&)xev;

#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("X11 button event :"
                    " type=%s, serial=%u, send_event=%d, display=%p, window=%p,"
                    " root=%p, subwindow=%p, time=%d, x=%d, y=%d, x_root=%d,"
                    " y_root=%d, state=%0X, button=%d, same_screen=%d",
                    xbev.type == ButtonPress ? "down" : "up", xbev.serial,
                    xbev.send_event, xbev.display, xbev.window, xbev.root,
                    xbev.subwindow, xbev.time, xbev.x, xbev.y, xbev.x_root,
                    xbev.y_root, xbev.state, xbev.button, xbev.same_screen);
#endif
                MirInputEventModifiers modifiers = mir_input_event_modifier_none;
                if (xbev.state & ShiftMask)
                    modifiers |= mir_input_event_modifier_shift;
                if (xbev.state & ControlMask)
                    modifiers |=  mir_input_event_modifier_ctrl;
                if (xbev.state & Mod1Mask)
                    modifiers |=  mir_input_event_modifier_alt;
                if (xbev.state & Mod4Mask)
                    modifiers |=  mir_input_event_modifier_meta;

                auto event_time =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::milliseconds{xbev.time});

                MirPointerButtons buttons_pressed = 0;
                if (xbev.state & Button1Mask)
                    buttons_pressed |= mir_pointer_button_primary;
                if (xbev.state & Button2Mask) // tertiary (middle) button is Button2 in X
                    buttons_pressed |= mir_pointer_button_tertiary;
                if (xbev.state & Button3Mask)
                    buttons_pressed |= mir_pointer_button_secondary;
                if (xbev.state & Button4Mask)
                    buttons_pressed |= mir_pointer_button_back;
                if (xbev.state & Button5Mask)
                    buttons_pressed |= mir_pointer_button_forward;

#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("Mir button event : x=%d, y=%d, "
                    "buttons_pressed=%0X, modifiers=%0X, event_time=%" PRId64,
                    xbev.x, xbev.y, buttons_pressed, modifiers, event_time);
#endif
                sink->handle_input(
                    *builder->pointer_event(
                        event_time,
                        modifiers,
                        xbev.type == ButtonPress ?
                            mir_pointer_action_button_down :
                            mir_pointer_action_button_up,
                        buttons_pressed,
                        xbev.x,
                        xbev.y,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f
                    )
                );
                break;
            }

            case MotionNotify:
            {
                auto const& xmev = (XMotionEvent&)xev;

#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("X11 motion event :"
                    " type=motion, serial=%u, send_event=%d, display=%p, window=%p,"
                    " root=%p, subwindow=%p, time=%d, x=%d, y=%d, x_root=%d,"
                    " y_root=%d, state=%0X, is_hint=%s, same_screen=%d",
                    xmev.serial, xmev.send_event, xmev.display, xmev.window,
                    xmev.root, xmev.subwindow, xmev.time, xmev.x, xmev.y, xmev.x_root,
                    xmev.y_root, xmev.state, xmev.is_hint == NotifyNormal ? "no" : "yes", xmev.same_screen);
#endif
                MirInputEventModifiers modifiers = mir_input_event_modifier_none;
                if (xmev.state & ShiftMask)
                    modifiers |= mir_input_event_modifier_shift;
                if (xmev.state & ControlMask)
                    modifiers |=  mir_input_event_modifier_ctrl;
                if (xmev.state & Mod1Mask)
                    modifiers |=  mir_input_event_modifier_alt;
                if (xmev.state & Mod4Mask)
                    modifiers |=  mir_input_event_modifier_meta;

                auto event_time =
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::milliseconds{xmev.time});

                MirPointerButtons buttons_pressed = 0;
                if (xmev.state & Button1Mask)
                    buttons_pressed |= mir_pointer_button_primary;
                if (xmev.state & Button2Mask) // tertiary (middle) button is Button2 in X
                    buttons_pressed |= mir_pointer_button_tertiary;
                if (xmev.state & Button3Mask)
                    buttons_pressed |= mir_pointer_button_secondary;
                if (xmev.state & Button4Mask)
                    buttons_pressed |= mir_pointer_button_back;
                if (xmev.state & Button5Mask)
                    buttons_pressed |= mir_pointer_button_forward;

#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("Mir pointer event : "
                    "x=%d, y=%d, buttons_pressed=%0X, modifiers=%0X, event_time=%" PRId64,
                    xmev.x, xmev.y, buttons_pressed, modifiers, event_time);
#endif
                sink->handle_input(
                    *builder->pointer_event(
                        event_time,
                        modifiers,
                        mir_pointer_action_motion,
                        buttons_pressed,
                        xmev.x,
                        xmev.y,
                        0.0f,
                        0.0f,
                        0.0f,
                        0.0f
                    )
                );
                break;
            }

            case ConfigureNotify:
            {
#ifdef MIR_ON_X11_INPUT_VERBOSE
                auto const& xcev = (XConfigureEvent&)xev;
                mir::log_info("Window size : %dx%d", xcev.width, xcev.height);
#endif
                break;
            }

            case MappingNotify:
#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("Keyboard mapping changed at server. Refreshing the cache.");
#endif
                XRefreshKeyboardMapping((XMappingEvent*)&xev);
                break;

            default:
#ifdef MIR_ON_X11_INPUT_VERBOSE
                mir::log_info("Uninteresting event : %08X", xev.type);
#endif
                break;
            }
        }
        else
            mir::log_error("input event received with no sink to handle it");
    }

    return ret;
}

md::FdEvents mix::XDispatchable::relevant_events() const
{
    return md::FdEvent::readable | md::FdEvent::error | md::FdEvent::remote_closed;
}

void mix::XDispatchable::set_input_sink(mi::InputSink* input_sink, mi::EventBuilder* event_builder)
{
    sink = input_sink;
    builder = event_builder;
}

void mix::XDispatchable::unset_input_sink()
{
    sink = nullptr;
    builder = nullptr;
}
