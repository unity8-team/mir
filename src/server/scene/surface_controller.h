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
        std::shared_ptr<shell::DisplayLayout> const& display_layout,
        std::shared_ptr<SurfaceStackModel> const& surface_stack);

    std::shared_ptr<Surface> add_surface(
        SurfaceCreationParameters const& params,
        Session* session) override;

    void remove_surface(std::weak_ptr<Surface> const& surface) override;

    void raise(std::weak_ptr<Surface> const& surface) override;

    /**
     * wrap_surface allows you wrap ("decorate") a surface in your own
     * window management policy-enforcing class. It's important to keep
     * the wrapping stage out of SurfaceFactory, so that a shell may choose
     * to both partly reuse the default wrapper (ManagedSurface) for
     * window management logic while also retaining access to the underlying
     * BasicSurface if it wants to do anything that would be disallowed by
     * the default policy of ManagedSurface.
     * TODO: Move this up to SurfaceCoordinator (or some bespoke factory)
     *       in the public API later.
     */
    virtual std::shared_ptr<Surface>
        wrap_surface(std::shared_ptr<Surface> const& raw);

private:
    std::shared_ptr<SurfaceFactory> const surface_factory;
    std::shared_ptr<PlacementStrategy> const placement_strategy;
    std::shared_ptr<shell::DisplayLayout> const display_layout;
    std::shared_ptr<SurfaceStackModel> const surface_stack;
};

}
}


#endif /* MIR_SCENE_SURFACE_CONTROLLER_H_ */
