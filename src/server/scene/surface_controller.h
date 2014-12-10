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
namespace scene
{
class PlacementStrategy;
class SurfaceStackModel;
class SurfaceFactory;

/// Will grow up to provide synchronization of model updates
class SurfaceController : public SurfaceCoordinator
{
public:
    SurfaceController(
        std::shared_ptr<SurfaceFactory> const& surface_factory,
        std::shared_ptr<PlacementStrategy> const& placement_strategy,
        std::shared_ptr<SurfaceStackModel> const& surface_stack);

    std::shared_ptr<Surface> add_surface(
        SurfaceCreationParameters const& params,
        Session* session) override;

    void remove_surface(std::weak_ptr<Surface> const& surface) override;

    void raise(std::weak_ptr<Surface> const& surface) override;

    /**
     * wrap_surface allows you wrap ("decorate") a surface in your own
     * window management policy-enforcing class. This facility is provided
     * in SurfaceController instead of the SurfaceFactory so that you still
     * have the option of discarding Mir's default window management policy
     * (from ManagedSurface) and can access the raw underlying Surface
     * object freely without any policy constraints. This design also means
     * your wrapped Surface class only needs a simple single argument to
     * construct (see SurfaceWrapper or inherit from ManagedSurface).
     */
    virtual std::shared_ptr<Surface>
        wrap_surface(std::shared_ptr<Surface> const& raw);

private:
    std::shared_ptr<SurfaceFactory> const surface_factory;
    std::shared_ptr<PlacementStrategy> const placement_strategy;
    std::shared_ptr<SurfaceStackModel> const surface_stack;
};

}
}


#endif /* MIR_SCENE_SURFACE_CONTROLLER_H_ */
