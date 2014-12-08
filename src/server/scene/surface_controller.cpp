/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 *              Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "surface_controller.h"
#include "surface_stack_model.h"
#include "mir/scene/surface_factory.h"
#include "mir/scene/surface.h"
#include "mir/scene/placement_strategy.h"
#include "mir/shell/display_layout.h"
#include "mir/geometry/point.h"
#include "mir/geometry/displacement.h"

namespace ms = mir::scene;

ms::SurfaceController::SurfaceController(
    std::shared_ptr<SurfaceFactory> const& surface_factory,
    std::shared_ptr<ms::PlacementStrategy> const& placement_strategy,
    std::shared_ptr<shell::DisplayLayout> const& display_layout,
    std::shared_ptr<SurfaceStackModel> const& surface_stack) :
    surface_factory(surface_factory),
    placement_strategy(placement_strategy),
    display_layout(display_layout),
    surface_stack(surface_stack)
{
}

std::shared_ptr<ms::Surface> ms::SurfaceController::add_surface(
    SurfaceCreationParameters const& params,
    Session* session)
{
    auto placed_params = placement_strategy->place(*session, params);

    auto const surface = surface_factory->create_surface(placed_params);
    surface_stack->add_surface(surface, placed_params.depth, placed_params.input_mode);
    return surface;
}

void ms::SurfaceController::remove_surface(std::weak_ptr<Surface> const& surface)
{
    surface_stack->remove_surface(surface);
}

void ms::SurfaceController::raise(std::weak_ptr<Surface> const& surface)
{
    surface_stack->raise(surface);
}

int ms::SurfaceController::configure_surface(Surface& surface,
                                             MirSurfaceAttrib attrib,
                                             int value)
{
    int new_value = surface.configure(attrib, value);

    if (attrib == mir_surface_attrib_state)
        set_state(surface, static_cast<MirSurfaceState>(new_value));

    return new_value;
}

void ms::SurfaceController::set_state(Surface& surface,
                                      MirSurfaceState desired)
{
    if (desired == mir_surface_state_minimized ||
        desired == mir_surface_state_restored)
        return;

    geometry::Rectangle new_win, old_win{surface.top_left(), surface.size()};

    auto fullscreen = old_win;
    display_layout->size_to_output(fullscreen);

    // TODO: Limit maximized to exclude panels/launchers defined by the shell
    auto maximized = fullscreen;

    switch (desired)
    {
    case mir_surface_state_fullscreen:
        new_win = fullscreen;
        break;
    case mir_surface_state_maximized:
        new_win = maximized;
        break;
    case mir_surface_state_vertmaximized:
        new_win.top_left.x = old_win.top_left.x;
        new_win.top_left.y = maximized.top_left.y;
        new_win.size.width = old_win.size.width;
        new_win.size.height = maximized.size.height;
        break;
    default:
        new_win = old_win;
        break;
    }

    if (old_win != new_win)
    {
        // TODO: Make these an atomic operation (LP: #1395957)
        surface.resize(new_win.size);       // Might throw
        surface.move_to(new_win.top_left);  // Unlikely to ever throw
    }
}

void ms::SurfaceController::drag_surface(Surface& surface,
                                         geometry::Displacement& grab,
                                         geometry::Point const& cursor)
{
    int const snap_distance = 50; // TODO: configurable
    bool unsnap = false;
    auto old_pos = surface.top_left();
    auto delta = (cursor - old_pos) - grab;
    auto new_pos = old_pos + delta;

    switch (surface.state())
    {
    case mir_surface_state_fullscreen:
        new_pos = old_pos;
        break;
    case mir_surface_state_maximized:
        if (delta.length_squared() >= (snap_distance * snap_distance))
            unsnap = true;
        else
            new_pos = old_pos;
        break;
    case mir_surface_state_vertmaximized:
        if (abs(delta.dy.as_int()) >= snap_distance)
            unsnap = true;
        else
            new_pos.y = old_pos.y;
        break;
    case mir_surface_state_restored:
        // TODO compare cursor to screen edges and semi/maximize as appropriate
        break;
    default:
        break;
    }

    if (unsnap)
    {
        configure_surface(surface, mir_surface_attrib_state,
                          mir_surface_state_restored);

        geometry::Rectangle restored{new_pos, surface.size()};
        if (!restored.contains(cursor))
        {
            grab = {restored.size.width.as_int()/2,
                    restored.size.height.as_int()/2};
            new_pos = cursor - grab;
        }
    }

    surface.move_to(new_pos);
}
