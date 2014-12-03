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

namespace ms = mir::scene;

ms::SurfaceController::SurfaceController(
    std::shared_ptr<SurfaceFactory> const& surface_factory,
    std::shared_ptr<ms::PlacementStrategy> const& placement_strategy,
    std::shared_ptr<SurfaceStackModel> const& surface_stack) :
    surface_factory(surface_factory),
    placement_strategy(placement_strategy),
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
    switch (attrib)
    {
    case mir_surface_attrib_state:
        set_state(surface, static_cast<MirSurfaceState>(value));
        break;
    default:
        break;
    }
    return surface.configure(attrib, value);
}

void ms::SurfaceController::set_state(Surface& surface,
                                      MirSurfaceState desired)
{
    switch (desired)
    {
    case mir_surface_state_fullscreen:
        fullscreen(surface);
        break;
    default:
        break;
    }
}

void ms::SurfaceController::fullscreen(Surface& surface)
{
    auto rect = placement_strategy->fullscreen({surface.top_left(),
                                                surface.size()});

    // TODO: Make these an atomic operation (LP: #1395957)
    surface.resize(rect.size);       // Might throw
    surface.move_to(rect.top_left);  // Unlikely to ever throw
}
