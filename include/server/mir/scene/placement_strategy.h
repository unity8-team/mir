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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_SCENE_PLACEMENT_STRATEGY_H_
#define MIR_SCENE_PLACEMENT_STRATEGY_H_

#include "mir/geometry/rectangle.h"

namespace mir
{
namespace scene
{
class Session;
struct SurfaceCreationParameters;

// TODO: Consider merging this into SurfaceCoordinator
class PlacementStrategy
{
public:
    virtual ~PlacementStrategy() = default;

    virtual SurfaceCreationParameters place(Session const& session, SurfaceCreationParameters const& request_parameters) = 0;

    virtual geometry::Rectangle fullscreen(geometry::Rectangle const&) const = 0;

protected:
    PlacementStrategy() = default;
    PlacementStrategy(PlacementStrategy const&) = delete;
    PlacementStrategy& operator=(PlacementStrategy const&) = delete;
};
}
} // namespace mir

#endif // MIR_SCENE_PLACEMENT_STRATEGY_H_
