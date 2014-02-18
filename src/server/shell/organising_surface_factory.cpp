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
 */

#include "organising_surface_factory.h"
#include "mir/shell/placement_strategy.h"
#include "mir/shell/surface_creation_parameters.h"

namespace mf = mir::frontend;
namespace msh = mir::shell;

msh::OrganisingSurfaceFactory::OrganisingSurfaceFactory(std::shared_ptr<msh::SurfaceFactory> const& underlying_factory,
                                                          std::shared_ptr<msh::PlacementStrategy> const& placement_strategy)
 : underlying_factory(underlying_factory),
   placement_strategy(placement_strategy)
{
}

msh::OrganisingSurfaceFactory::~OrganisingSurfaceFactory()
{
}

std::shared_ptr<msh::Surface> msh::OrganisingSurfaceFactory::create_surface(
    Session* session,
    shell::SurfaceCreationParameters const& params,
    frontend::SurfaceId id,
    std::shared_ptr<mf::EventSink> const& sender)
{
    shell::SurfaceCreationParameters placed = params;

    // TODO: Remove this version of place(). It only exists to initialize
    //       "depth" as required by some sticky test cases.
    placement_strategy->place(placed);
    auto surf = underlying_factory->create_surface(session, placed, id, sender);
    placement_strategy->place(*surf);
    return surf;
}

