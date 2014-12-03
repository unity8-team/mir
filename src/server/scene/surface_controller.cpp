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
 */

#include "surface_controller.h"
#include "surface_stack_model.h"
#include "mir/scene/surface_factory.h"
#include "mir/scene/surface.h"
#include "mir/scene/placement_strategy.h"
#include "mir/shell/display_layout.h"

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
    if (attrib == mir_surface_attrib_state)
        set_state(surface, static_cast<MirSurfaceState>(value));

    return surface.configure(attrib, value);
}

void ms::SurfaceController::set_state(Surface& surface,
                                      MirSurfaceState desired)
{
    if (desired == mir_surface_state_minimized ||
        desired == mir_surface_state_restored)
        return;

    geometry::Rectangle old_win{surface.top_left(), surface.size()};
    auto new_win = old_win;

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
        // new_win.top_left.x is unchanged
        new_win.top_left.y = maximized.top_left.y;
        // new_win.size.width is unchanged
        new_win.size.height = maximized.size.height;
        break;
    default:
        break;
    }

    if (old_win != new_win)
    {
        // TODO: Make these an atomic operation (LP: #1395957)
        surface.resize(new_win.size);       // Might throw
        surface.move_to(new_win.top_left);  // Unlikely to ever throw
    }
}

