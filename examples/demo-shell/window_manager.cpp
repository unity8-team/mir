/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 *              Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "window_manager.h"

#include "mir/shell/focus_controller.h"
#include "session_container.h"
#include "mir/scene/session.h"
#include "mir/scene/surface.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_configuration.h"
#include "mir/compositor/compositor.h"
#include "mir/options/configuration.h"

#include <linux/input.h>
#include <android/keycodes.h>  // TODO remove this dependency

#include <cassert>
#include <cstdlib>
#include <cmath>

#include <sys/types.h>
#include <unistd.h>

namespace me = mir::examples;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace ms = mir::scene;
namespace mo = mir::options;

namespace
{
const int min_swipe_distance = 100;  // How long must a swipe be to act on?
}

me::WindowManager::WindowManager()
    : old_pinch_diam(0.0f), max_fingers(0)
{
}

void me::WindowManager::set_focus_controller(std::shared_ptr<msh::FocusController> const& controller)
{
    focus_controller = controller;
}

void me::WindowManager::set_display(std::shared_ptr<mg::Display> const& dpy)
{
    display = dpy;
}

void me::WindowManager::set_compositor(std::shared_ptr<mc::Compositor> const& cptor)
{
    compositor = cptor;
}

void me::WindowManager::set_session_container(std::shared_ptr<ms::SessionContainer> const& sc)
{
	session_container = sc;
}

void me::WindowManager::set_options(
		std::shared_ptr<mo::Option> const& o)
{
	options = o;
}

namespace
{

mir::geometry::Point average_pointer(MirMotionEvent const& motion)
{
    using namespace mir;
    using namespace geometry;

    int x = 0, y = 0, count = static_cast<int>(motion.pointer_count);

    for (int i = 0; i < count; i++)
    {
        x += motion.pointer_coordinates[i].x;
        y += motion.pointer_coordinates[i].y;
    }

    x /= count;
    y /= count;

    return Point{x, y};
}

float measure_pinch(MirMotionEvent const& motion,
                    mir::geometry::Displacement& dir)
{
    int count = static_cast<int>(motion.pointer_count);
    int max = 0;

    for (int i = 0; i < count; i++)
    {
        for (int j = 0; j < i; j++)
        {
            int dx = motion.pointer_coordinates[i].x -
                     motion.pointer_coordinates[j].x;
            int dy = motion.pointer_coordinates[i].y -
                     motion.pointer_coordinates[j].y;

            int sqr = dx*dx + dy*dy;

            if (sqr > max)
            {
                max = sqr;
                dir = mir::geometry::Displacement{dx, dy};
            }
        }
    }

    return sqrtf(max);  // return pinch diameter
}

} // namespace

bool me::WindowManager::handle(MirEvent const& event)
{
    // TODO: Fix android configuration and remove static hack ~racarr
    static bool display_off = false;
    assert(focus_controller);
    assert(display);
    assert(compositor);

    bool handled = false;

    if (event.key.type == mir_event_type_key &&
        event.key.action == mir_key_action_down)
    {
        if (event.key.modifiers & mir_key_modifier_alt &&
            event.key.scan_code == KEY_TAB)  // TODO: Use keycode once we support keymapping on the server side
        {
            focus_controller->focus_next();
            return true;
        }
        else if ((event.key.modifiers & mir_key_modifier_alt &&
                  event.key.scan_code == KEY_L))
        {
            if (!options->is_set(mo::host_socket_opt))
            {
                printf("[Host]: Triggering LC callback\r\n");
				session_container->for_each(
							[](std::shared_ptr<ms::Session> const& session)
							{
								session->set_lifecycle_state(mir_lifecycle_state_will_suspend);
							});
            }
        }
        else if ((event.key.modifiers & mir_key_modifier_alt &&
                  event.key.scan_code == KEY_P) ||
                 (event.key.key_code == AKEYCODE_POWER))
        {
            compositor->stop();
            auto conf = display->configuration();
            MirPowerMode new_power_mode = display_off ?
                mir_power_mode_on : mir_power_mode_off;
            conf->for_each_output(
                [&](mg::UserDisplayConfigurationOutput& output) -> void
                {
                    output.power_mode = new_power_mode;
                }
            );
            display_off = !display_off;

            display->configure(*conf.get());
            if (!display_off)
                compositor->start();
            return true;
        }
        else if ((event.key.modifiers & mir_key_modifier_alt) &&
                 (event.key.modifiers & mir_key_modifier_ctrl) &&
                 (event.key.scan_code == KEY_ESC))
        {
            std::abort();
            return true;
        }
        else if ((event.key.modifiers & mir_key_modifier_alt) &&
                 (event.key.modifiers & mir_key_modifier_ctrl))
        {
            MirOrientation orientation = mir_orientation_normal;
            bool rotating = true;
            int mode_change = 0;
            bool preferred_mode = false;

            switch (event.key.scan_code)
            {
            case KEY_UP:    orientation = mir_orientation_normal; break;
            case KEY_DOWN:  orientation = mir_orientation_inverted; break;
            case KEY_LEFT:  orientation = mir_orientation_left; break;
            case KEY_RIGHT: orientation = mir_orientation_right; break;
            default:        rotating = false; break;
            }

            switch (event.key.scan_code)
            {
            case KEY_MINUS: mode_change = -1;      break;
            case KEY_EQUAL: mode_change = +1;      break;
            case KEY_0:     preferred_mode = true; break;
            default:                               break;
            }

            if (rotating || mode_change || preferred_mode)
            {
                compositor->stop();
                auto conf = display->configuration();
                conf->for_each_output(
                    [&](mg::UserDisplayConfigurationOutput& output) -> void
                    {
                        // Only apply changes to the monitor the cursor is on
                        if (!output.extents().contains(old_cursor))
                            return;

                        if (rotating)
                            output.orientation = orientation;

                        if (preferred_mode)
                        {
                            output.current_mode_index =
                                output.preferred_mode_index;
                        }
                        else if (mode_change)
                        {
                            size_t nmodes = output.modes.size();
                            if (nmodes)
                                output.current_mode_index =
                                    (output.current_mode_index + nmodes +
                                     mode_change) % nmodes;
                        }
                    }
                );
                display->configure(*conf);
                compositor->start();
                return true;
            }
        }

    }
    else if (event.type == mir_event_type_motion &&
             focus_controller)
    {
        geometry::Point cursor = average_pointer(event.motion);

        // FIXME: https://bugs.launchpad.net/mir/+bug/1311699
        MirMotionAction action = static_cast<MirMotionAction>(event.motion.action & ~0xff00);

        auto const app = focus_controller->focussed_application().lock();

        int fingers = static_cast<int>(event.motion.pointer_count);

        if (action == mir_motion_action_down || fingers > max_fingers)
            max_fingers = fingers;

        if (app)
        {
            // FIXME: We need to be able to select individual surfaces in
            //        future and not just the "default" one.
            auto const surf = app->default_surface();

            if (surf &&
                (event.motion.modifiers & mir_key_modifier_alt ||
                 fingers >= 3))
            {
                geometry::Displacement pinch_dir;
                auto pinch_diam =
                    measure_pinch(event.motion, pinch_dir);

                // Start of a gesture: When the latest finger/button goes down
                if (action == mir_motion_action_down ||
                    action == mir_motion_action_pointer_down)
                {
                    click = cursor;
                    handled = true;
                }
                else if (event.motion.action == mir_motion_action_move &&
                         max_fingers <= 3)  // Avoid accidental movement
                {
                    geometry::Displacement drag = cursor - old_cursor;

                    if (event.motion.button_state ==
                        mir_motion_button_tertiary)
                    {  // Resize by mouse middle button
                        int width = old_size.width.as_int() +
                                    drag.dx.as_int();
                        int height = old_size.height.as_int() +
                                     drag.dy.as_int();
                        surf->resize({width, height});
                    }
                    else
                    { // Move surface (by mouse or 3 fingers)
                        surf->move_to(old_pos + drag);
                    }

                    if (fingers == 3)
                    {  // Resize by pinch/zoom
                        float diam_delta = pinch_diam - old_pinch_diam;
                        /*
                         * Resize vector (dx,dy) has length=diam_delta and
                         * direction=pinch_dir, so solve for (dx,dy)...
                         */
                        float lenlen = diam_delta * diam_delta;
                        int x = pinch_dir.dx.as_int();
                        int y = pinch_dir.dy.as_int();
                        int xx = x * x;
                        int yy = y * y;
                        int xxyy = xx + yy;
                        int dx = sqrtf(lenlen * xx / xxyy);
                        int dy = sqrtf(lenlen * yy / xxyy);
                        if (diam_delta < 0.0f)
                        {
                            dx = -dx;
                            dy = -dy;
                        }

                        int width = old_size.width.as_int() + dx;
                        int height = old_size.height.as_int() + dy;
                        surf->resize({width, height});
                    }

                    handled = true;
                }
                else if (action == mir_motion_action_scroll)
                {
                    float alpha = surf->alpha();
                    alpha += 0.1f *
                             event.motion.pointer_coordinates[0].vscroll;
                    if (alpha < 0.0f)
                        alpha = 0.0f;
                    else if (alpha > 1.0f)
                        alpha = 1.0f;
                    surf->set_alpha(alpha);
                    handled = true;
                }

                old_pos = surf->top_left();
                old_size = surf->size();
                old_pinch_diam = pinch_diam;
            }
        }

        if (max_fingers == 4 && action == mir_motion_action_up)
        { // Four fingers released
            geometry::Displacement dir = cursor - click;
            if (abs(dir.dx.as_int()) >= min_swipe_distance)
            {
                focus_controller->focus_next();
                handled = true;
            }
        }

        old_cursor = cursor;
    }
    return handled;
}
