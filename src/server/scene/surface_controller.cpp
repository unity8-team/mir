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
                                         geometry::Displacement const& grab,
                                         geometry::Point const& cursor)
{
    int const snap_distance = 30; // TODO: configurable
    int const sqr_snap_distance = snap_distance * snap_distance;

    auto const& pos = surface.top_left();

    // Local = current relative coordinate of the cursor in the surface
    int local_x = cursor.x.as_int() - pos.x.as_int();
    int local_y = cursor.y.as_int() - pos.y.as_int();

    // Delta = the drag gesture vector
    int dx = local_x - grab.dx.as_int();
    int dy = local_y - grab.dy.as_int();

    switch (surface.state())
    {
    case mir_surface_state_maximized:
    case mir_surface_state_fullscreen:
        if ((dx*dx + dy*dy) >= sqr_snap_distance)
            configure_surface(surface, mir_surface_attrib_state,
                              mir_surface_state_restored);
        break;
    case mir_surface_state_vertmaximized:
        if ((dy*dy) >= sqr_snap_distance)
            configure_surface(surface, mir_surface_attrib_state,
                              mir_surface_state_restored);
        else
            surface.move_to({pos.x.as_int() + dx, pos.y});
        break;
    case mir_surface_state_restored:
        {
        surface.move_to(pos + geometry::Displacement{dx,dy});

        // FIXME LP: #1398294
#if 0
        geometry::Rectangle workarea{pos, surface.size()};
        display_layout->size_to_output(workarea);  // TODO implement workarea
    
        // Drag to top of screen: maximize
        if (cursor.y <= workarea.top_left.y)
        {
            configure_surface(surface, mir_surface_attrib_state,
                              mir_surface_state_maximized);
        }
    
        // Drag to bottom: vertmaximize
        if (cursor.y.as_int() >= workarea.bottom_right().y.as_int() - 1)
        {
            configure_surface(surface, mir_surface_attrib_state,
                              mir_surface_state_vertmaximized);
        }
#endif
        break;
        }
    default:
        break;
    }
}
