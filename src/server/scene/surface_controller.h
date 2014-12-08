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


#ifndef MIR_SCENE_SURFACE_CONTROLLER_H_
#define MIR_SCENE_SURFACE_CONTROLLER_H_

#include "mir/scene/surface_coordinator.h"

namespace mir
{

namespace shell { class DisplayLayout; }
namespace geometry { struct Point; }

namespace scene
{
class PlacementStrategy;
class SurfaceStackModel;
class SurfaceFactory;

/**
 * SurfaceController is the default implementation of SurfaceCoordinator.
 * It provides a significant set of default window management behaviours that
 * we expect many shells will want to reuse. Surface objects by themselves
 * are designed to be self-contained an unaware of their surroundings, so
 * SurfaceController/Coordinator has an important job to fill in the blanks
 * such as telling a surface how to go full screen, maximize, or (in future)
 * how to snap to an adjacent surface.
 *   SurfaceController must provide all the window management logic that a
 * Surface object by itself can't fulfil.
 */
class SurfaceController : public SurfaceCoordinator
{
public:
    SurfaceController(
        std::shared_ptr<SurfaceFactory> const& surface_factory,
        std::shared_ptr<PlacementStrategy> const& placement_strategy,
        std::shared_ptr<shell::DisplayLayout> const& display_layout,
        std::shared_ptr<SurfaceStackModel> const& surface_stack);

    std::shared_ptr<Surface> add_surface(
        SurfaceCreationParameters const& params,
        Session* session) override;

    void remove_surface(std::weak_ptr<Surface> const& surface) override;

    void raise(std::weak_ptr<Surface> const& surface) override;

    int configure_surface(Surface&, MirSurfaceAttrib, int) override;
    void drag_surface(Surface& surf,
                      geometry::Displacement& grab,
                      geometry::Point const& cursor) override;

private:
    std::shared_ptr<SurfaceFactory> const surface_factory;
    std::shared_ptr<PlacementStrategy> const placement_strategy;
    std::shared_ptr<shell::DisplayLayout> const display_layout;
    std::shared_ptr<SurfaceStackModel> const surface_stack;

    void set_state(Surface&, MirSurfaceState);
};

}
}


#endif /* MIR_SCENE_SURFACE_CONTROLLER_H_ */
