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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "server_example_canonical_window_manager.h"

#include "mir/scene/surface.h"
#include "mir/scene/surface_coordinator.h"
#include "mir/geometry/displacement.h"

#include <linux/input.h>
#include <csignal>

namespace me = mir::examples;
namespace ms = mir::scene;
using namespace mir::geometry;

///\example server_example_canonical_window_manager.cpp
// Based on "Mir and Unity: Surfaces, input, and displays (v0.3)"

namespace
{
int const title_bar_height = 10;
}

me::CanonicalSurfaceInfo::CanonicalSurfaceInfo(
    std::shared_ptr<scene::Session> const& session,
    std::shared_ptr<scene::Surface> const& surface) :
    state{mir_surface_state_restored},
    restore_rect{surface->top_left(), surface->size()},
    session{session},
    parent{surface->parent()}
{
}

me::CanonicalWindowManagerPolicy::CanonicalWindowManagerPolicy(Tools* const tools,
    std::shared_ptr<scene::SurfaceCoordinator> const& surface_coordinator) :
    tools{tools}, surface_coordinator{surface_coordinator}
{
}

void me::CanonicalWindowManagerPolicy::click(Point cursor)
{
    if (auto const surface = surface_coordinator->surface_at(cursor))
        select_active_surface(surface);

    old_cursor = cursor;
}

void me::CanonicalWindowManagerPolicy::handle_session_info_updated(CanonicalSessionInfoMap& /*session_info*/, Rectangles const& /*displays*/)
{
}

void me::CanonicalWindowManagerPolicy::handle_displays_updated(CanonicalSessionInfoMap& /*session_info*/, Rectangles const& displays)
{
    display_area = displays.bounding_rectangle();
}

void me::CanonicalWindowManagerPolicy::resize(Point cursor)
{
    select_active_surface(surface_coordinator->surface_at(old_cursor));
    resize(active_surface(), cursor, old_cursor, display_area);
    old_cursor = cursor;
}

auto me::CanonicalWindowManagerPolicy::handle_place_new_surface(
    std::shared_ptr<ms::Session> const& session,
    ms::SurfaceCreationParameters const& request_parameters)
-> ms::SurfaceCreationParameters
{
    auto parameters = request_parameters;

    auto width = std::min(display_area.size.width.as_int(), parameters.size.width.as_int());
    auto height = std::min(display_area.size.height.as_int(), parameters.size.height.as_int());
    parameters.size = Size{width, height};

    bool positioned = false;

    auto const parent = parameters.parent.lock();

    if (!parent) // No parent => client can't suggest positioning
    {
        if (auto const default_surface = session->default_surface())
        {
            // "If an app does not suggest a position for a regular surface when opening
            // it, and the app has at least one regular surface already open, and there
            // is room to do so, Mir should place it one title bar’s height below and to
            // the right (in LTR languages) or to the left (in RTL languages) of the app’s
            // most recently active window, so that you can see the title bars of both."
            static Displacement const offset{title_bar_height, title_bar_height};

            parameters.top_left = default_surface->top_left() + offset;

            // "If there is not room to do that, Mir should place it as if it was the app’s
            // only regular surface."
            positioned = display_area.contains(parameters.top_left + as_displacement(parameters.size));
        }
    }

    if (parent && parameters.aux_rect.is_set() && parameters.edge_attachment.is_set())
    {
        auto const edge_attachment = parameters.edge_attachment.value();
        auto const aux_rect = parameters.aux_rect.value();
        auto const top_left = aux_rect.top_left + (parent->top_left() - display_area.top_left);
        auto const top_right= top_left + Displacement{aux_rect.size.width.as_int(), 0};
        auto const bot_left = top_left + Displacement{0, aux_rect.size.height.as_int()};

        if (edge_attachment && mir_edge_attachment_vertical)
        {
            if (display_area.contains(top_right + Displacement{width, height}))
            {
                parameters.top_left = top_right;
                positioned = true;
            }
            else if (display_area.contains(top_left + Displacement{-width, height}))
            {
                parameters.top_left = top_left + Displacement{-width, 0};
                positioned = true;
            }
        }

        if (edge_attachment && mir_edge_attachment_horizontal)
        {
            if (display_area.contains(bot_left + Displacement{width, height}))
            {
                parameters.top_left = bot_left;
                positioned = true;
            }
            else if (display_area.contains(top_left + Displacement{width, -height}))
            {
                parameters.top_left = top_left + Displacement{0, -height};
                positioned = true;
            }
        }
    }

    if (!positioned)
    {
        // "If an app does not suggest a position for its only regular surface when
        // opening it, Mir should position it horizontally centered, and vertically
        // such that the top margin is half the bottom margin. (Vertical centering
        // would look too low, and would allow little room for cascading.)"
        auto centred = display_area.top_left + 0.5*(
            as_displacement(display_area.size) - as_displacement(parameters.size));

        parameters.top_left = centred - Displacement{0, (display_area.size.height.as_int()-height)/6};
    }

    return parameters;
}

void me::CanonicalWindowManagerPolicy::handle_new_surface(std::shared_ptr<ms::Session> const& session, std::shared_ptr<ms::Surface> const& surface)
{
    if (auto const parent = surface->parent())
    {
        tools->info_for(parent).children.push_back(surface);
    }

    tools->info_for(session).surfaces++;

    // TODO There's currently no way to insert surfaces into an active (or inactive)
    // TODO window tree while keeping the order stable or consistent with spec.
    // TODO Nor is there a way to update the "default surface" when appropriate!!
    // TODO for now just give the surface's session focus.
    tools->set_focus_to(session);
}

void me::CanonicalWindowManagerPolicy::handle_delete_surface(std::shared_ptr<ms::Session> const& session, std::weak_ptr<ms::Surface> const& surface)
{
    if (auto const parent = tools->info_for(surface).parent.lock())
    {
        auto& siblings = tools->info_for(parent).children;

        for (auto i = begin(siblings); i != end(siblings); ++i)
        {
            if (surface.lock() == i->lock())
            {
                siblings.erase(i);
                break;
            }
        }
    }


    if (!--tools->info_for(session).surfaces && session == tools->focussed_application().lock())
    {
        tools->focus_next();
    }
}

int me::CanonicalWindowManagerPolicy::handle_set_state(std::shared_ptr<ms::Surface> const& surface, MirSurfaceState value)
{
    auto& info = tools->info_for(surface);

    switch (value)
    {
    case mir_surface_state_restored:
    case mir_surface_state_maximized:
    case mir_surface_state_vertmaximized:
    case mir_surface_state_horizmaximized:
        break;

    default:
        return info.state;
    }

    if (info.state == mir_surface_state_restored)
    {
        info.restore_rect = {surface->top_left(), surface->size()};
    }

    if (info.state == value)
    {
        return info.state;
    }

    auto const old_pos = surface->top_left();
    Displacement movement;

    switch (value)
    {
    case mir_surface_state_restored:
        movement = info.restore_rect.top_left - old_pos;
        surface->resize(info.restore_rect.size);
        break;

    case mir_surface_state_maximized:
        movement = display_area.top_left - old_pos;
        surface->resize(display_area.size);
        break;

    case mir_surface_state_horizmaximized:
        movement = Point{display_area.top_left.x, info.restore_rect.top_left.y} - old_pos;
        surface->resize({display_area.size.width, info.restore_rect.size.height});
        break;

    case mir_surface_state_vertmaximized:
        movement = Point{info.restore_rect.top_left.x, display_area.top_left.y} - old_pos;
        surface->resize({info.restore_rect.size.width, display_area.size.height});
        break;

    default:
        break;
    }

    // TODO It is rather simplistic to move a tree WRT the top_left of the root
    // TODO when resizing. But for more sophistication we would need to encode
    // TODO some sensible layout rules.
    move_tree(surface, movement);

    return info.state = value;
}

void me::CanonicalWindowManagerPolicy::drag(Point cursor)
{
    select_active_surface(surface_coordinator->surface_at(old_cursor));
    drag(active_surface(), cursor, old_cursor, display_area);
    old_cursor = cursor;
}

bool me::CanonicalWindowManagerPolicy::handle_key_event(MirKeyInputEvent const* event)
{
    auto const action = mir_key_input_event_get_action(event);
    auto const scan_code = mir_key_input_event_get_scan_code(event);
    auto const modifiers = mir_key_input_event_get_modifiers(event) & modifier_mask;

    if (action == mir_key_input_event_action_down && scan_code == KEY_F11)
    {
        switch (modifiers & modifier_mask)
        {
        case mir_input_event_modifier_alt:
            toggle(mir_surface_state_maximized);
            return true;

        case mir_input_event_modifier_shift:
            toggle(mir_surface_state_vertmaximized);
            return true;

        case mir_input_event_modifier_ctrl:
            toggle(mir_surface_state_horizmaximized);
            return true;

        default:
            break;
        }
    }
    else if (action == mir_key_input_event_action_down && scan_code == KEY_F4)
    {
        if (auto const session = tools->focussed_application().lock())
        {
            switch (modifiers & modifier_mask)
            {
            case mir_input_event_modifier_alt:
                kill(session->process_id(), SIGTERM);
                return true;

            case mir_input_event_modifier_ctrl:
                if (auto const surf = session->default_surface())
                {
                    surf->request_client_surface_close();
                    return true;
                }

            default:
                break;
            }
        }
    }

    return false;
}

bool me::CanonicalWindowManagerPolicy::handle_touch_event(MirTouchInputEvent const* event)
{
    auto const count = mir_touch_input_event_get_touch_count(event);

    long total_x = 0;
    long total_y = 0;

    for (auto i = 0U; i != count; ++i)
    {
        total_x += mir_touch_input_event_get_touch_axis_value(event, i, mir_touch_input_axis_x);
        total_y += mir_touch_input_event_get_touch_axis_value(event, i, mir_touch_input_axis_y);
    }

    Point const cursor{total_x/count, total_y/count};

    bool is_drag = true;
    for (auto i = 0U; i != count; ++i)
    {
        switch (mir_touch_input_event_get_touch_action(event, i))
        {
        case mir_touch_input_event_action_up:
            return false;

        case mir_touch_input_event_action_down:
            is_drag = false;

        case mir_touch_input_event_action_change:
            continue;
        }
    }

    if (is_drag && count == 3)
    {
        drag(cursor);
        return true;
    }
    else
    {
        click(cursor);
        return false;
    }
}

bool me::CanonicalWindowManagerPolicy::handle_pointer_event(MirPointerInputEvent const* event)
{
    auto const action = mir_pointer_input_event_get_action(event);
    auto const modifiers = mir_pointer_input_event_get_modifiers(event) & modifier_mask;
    Point const cursor{
        mir_pointer_input_event_get_axis_value(event, mir_pointer_input_axis_x),
        mir_pointer_input_event_get_axis_value(event, mir_pointer_input_axis_y)};

    if (action == mir_pointer_input_event_action_button_down)
    {
        click(cursor);
        return false;
    }
    else if (action == mir_pointer_input_event_action_motion &&
             modifiers == mir_input_event_modifier_alt)
    {
        if (mir_pointer_input_event_get_button_state(event, mir_pointer_input_button_primary))
        {
            drag(cursor);
            return true;
        }

        if (mir_pointer_input_event_get_button_state(event, mir_pointer_input_button_tertiary))
        {
            resize(cursor);
            return true;
        }
    }

    return false;
}

void me::CanonicalWindowManagerPolicy::toggle(MirSurfaceState state)
{
    if (auto const surface = active_surface())
    {
        if (surface->state() == state)
            state = mir_surface_state_restored;

        auto const value = handle_set_state(surface, MirSurfaceState(state));
        surface->configure(mir_surface_attrib_state, value);
    }
}

void me::CanonicalWindowManagerPolicy::select_active_surface(std::shared_ptr<ms::Surface> const& surface)
{
    if (!surface)
    {
        active_surface_.reset();
        return;
    }

    auto const& info_for = tools->info_for(surface);
    tools->set_focus_to(info_for.session.lock());

    // TODO There's currently no way to raise the active window tree while keeping
    // TODO the order stable or consistent with spec.
    // TODO This is definitely a frig that needs rework
    surface_coordinator->raise(surface);
    for (auto const& child : info_for.children)
        surface_coordinator->raise(child);

    switch (surface->type())
    {
    case mir_surface_type_normal:       /**< AKA "regular"                       */
    case mir_surface_type_utility:      /**< AKA "floating"                      */
    case mir_surface_type_dialog:
    case mir_surface_type_satellite:    /**< AKA "toolbox"/"toolbar"             */
    case mir_surface_type_freestyle:
    case mir_surface_type_menu:
    case mir_surface_type_inputmethod:  /**< AKA "OSK" or handwriting etc.       */
        // TODO set the input focus to this window
        active_surface_ = surface;
        break;

    case mir_surface_type_gloss:
    case mir_surface_type_tip:          /**< AKA "tooltip"                       */
    default:
        // Cannot have input focus
        if (auto const parent = info_for.parent.lock())
            select_active_surface(parent);
        break;
    }
}

auto me::CanonicalWindowManagerPolicy::active_surface() const
-> std::shared_ptr<ms::Surface>
{
    if (auto const surface = active_surface_.lock())
        return surface;

    if (auto const session = tools->focussed_application().lock())
    {
        if (auto const surface = session->default_surface())
            return surface;
    }

    return std::shared_ptr<ms::Surface>{};
}

bool me::CanonicalWindowManagerPolicy::resize(std::shared_ptr<ms::Surface> const& surface, Point cursor, Point old_cursor, Rectangle bounds)
{
    if (!surface || !surface->input_area_contains(cursor))
        return false;

    auto const top_left = surface->top_left();
    auto const old_size = surface->size();

    auto anchor = top_left;

    for (auto const& corner : {
        anchor + as_displacement(surface->size()),
        anchor + Displacement{surface->size().width.as_int(), 0},
        anchor + Displacement{0, surface->size().height.as_int()}})
    {
        if ((old_cursor - anchor).length_squared() <
            (old_cursor - corner).length_squared())
        {
            anchor = corner;
        }
    }

    bool const left_resize = anchor.x != top_left.x;
    bool const top_resize  = anchor.y != top_left.y;
    int const x_sign = left_resize? -1 : 1;
    int const y_sign = top_resize?  -1 : 1;

    auto const delta = cursor-old_cursor;

    Size new_size{
        old_size.width.as_int()  + x_sign*delta.dx.as_int(),
        old_size.height.as_int() + y_sign*delta.dy.as_int()};

    Point new_pos = top_left +
        Displacement{left_resize*delta.dx, top_resize*delta.dy};

    if (left_resize)
    {
        if (new_pos.x < bounds.top_left.x)
        {
            new_size.width = Width{new_size.width.as_int() + (new_pos.x - bounds.top_left.x).as_int()};
            new_pos.x = bounds.top_left.x;
        }
    }
    else
    {
        auto to_bottom_right = bounds.bottom_right() - (new_pos + as_displacement(new_size));
        if (to_bottom_right.dx < DeltaX{0})
            new_size.width = Width{new_size.width.as_int() + to_bottom_right.dx.as_int()};
    }

    if (top_resize)
    {
        if (new_pos.y < bounds.top_left.y)
        {
            new_size.height = Height{new_size.height.as_int() + (new_pos.y - bounds.top_left.y).as_int()};
            new_pos.y = bounds.top_left.y;
        }
    }
    else
    {
        auto to_bottom_right = bounds.bottom_right() - (new_pos + as_displacement(new_size));
        if (to_bottom_right.dy < DeltaY{0})
            new_size.height = Height{new_size.height.as_int() + to_bottom_right.dy.as_int()};
    }

    surface->resize(new_size);

    // TODO It is rather simplistic to move a tree WRT the top_left of the root
    // TODO when resizing. But for more sophistication we would need to encode
    // TODO some sensible layout rules.
    move_tree(surface, new_pos-top_left);

    return true;
}

bool me::CanonicalWindowManagerPolicy::drag(std::shared_ptr<ms::Surface> surface, Point to, Point from, Rectangle bounds)
{
    if (surface && surface->input_area_contains(from))
    {
        auto const top_left = surface->top_left();
        auto const surface_size = surface->size();
        auto const bottom_right = top_left + as_displacement(surface_size);

        auto movement = to - from;

        if (movement.dx < DeltaX{0})
            movement.dx = std::max(movement.dx, (bounds.top_left - top_left).dx);

        if (movement.dy < DeltaY{0})
            movement.dy = std::max(movement.dy, (bounds.top_left - top_left).dy);

        if (movement.dx > DeltaX{0})
            movement.dx = std::min(movement.dx, (bounds.bottom_right() - bottom_right).dx);

        if (movement.dy > DeltaY{0})
            movement.dy = std::min(movement.dy, (bounds.bottom_right() - bottom_right).dy);

        move_tree(surface, movement);

        return true;
    }

    return false;
}

void me::CanonicalWindowManagerPolicy::move_tree(std::shared_ptr<ms::Surface> const& root, Displacement movement) const
{
    root->move_to(root->top_left() + movement);

    for (auto const& child: tools->info_for(root).children)
    {
        move_tree(child.lock(), movement);
    }
}
