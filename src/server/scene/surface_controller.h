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

#include "surface_builder.h"
#include "surface_ranker.h"

namespace mir
{
namespace shell
{
class Session;
}

namespace scene
{
class SurfaceStackModel;

/// Will grow up to provide synchronization of model updates
class SurfaceController : public SurfaceBuilder, public SurfaceRanker
{
public:
    explicit SurfaceController(std::shared_ptr<SurfaceStackModel> const& surface_stack);

    std::weak_ptr<BasicSurface> create_surface(
        frontend::SurfaceId id,
        shell::SurfaceCreationParameters const& params,
        std::shared_ptr<frontend::EventSink> const& event_sink,
        std::shared_ptr<shell::SurfaceConfigurator> const& configurator) override;

    void destroy_surface(std::weak_ptr<BasicSurface> const& surface) override;

    void raise(std::weak_ptr<BasicSurface> const& surface) override;

private:
    std::shared_ptr<SurfaceStackModel> const surface_stack;
};

}
}


#endif /* MIR_SCENE_SURFACE_CONTROLLER_H_ */
