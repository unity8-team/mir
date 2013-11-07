/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// local
#include "initialsurfaceplacementstrategy.h"

// mir
#include "mir/shell/surface_creation_parameters.h"
#include "mir/geometry/rectangle.h"

namespace msh = mir::shell;

InitialSurfacePlacementStrategy::InitialSurfacePlacementStrategy(std::shared_ptr<msh::DisplayLayout> const& displayLayout)
  : m_displayLayout(displayLayout)
{
}

msh::SurfaceCreationParameters
InitialSurfacePlacementStrategy::place(msh::Session const& /*session*/, msh::SurfaceCreationParameters const& requestParameters)
{
    using namespace mir::geometry;
    auto placedParameters = requestParameters;

    Rectangle rect{placedParameters.top_left, placedParameters.size};
    m_displayLayout->size_to_output(rect);
    placedParameters.size = rect.size;

    // position surface initially off-screen, until ready to be animated in
//    placedParameters.top_left = Point{ X{rect.size.width.as_int()}, Y{0}};

    return placedParameters;
}
