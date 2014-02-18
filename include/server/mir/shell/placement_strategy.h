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

#ifndef MIR_SHELL_PLACEMENT_STRATEGY_H_
#define MIR_SHELL_PLACEMENT_STRATEGY_H_

namespace mir
{

namespace shell
{
struct SurfaceCreationParameters;
class Surface;

class PlacementStrategy
{
public:
    virtual ~PlacementStrategy() {}

    // TODO: Deprecate the SurfaceCreationParameters version. It only exists
    //       to support legacy test cases that explicitly set stacking depths.
    virtual void place(SurfaceCreationParameters&) const {}
    virtual void place(Surface& surface) const = 0;

protected:
    PlacementStrategy() = default;
    PlacementStrategy(PlacementStrategy const&) = delete;
    PlacementStrategy& operator=(PlacementStrategy const&) = delete;
};

}
} // namespace mir

#endif // MIR_SHELL_PLACEMENT_STRATEGY_H_
