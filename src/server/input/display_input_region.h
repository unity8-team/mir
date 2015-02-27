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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_INPUT_DISPLAY_INPUT_REGION_H_
#define MIR_INPUT_DISPLAY_INPUT_REGION_H_

#include "mir/input/input_region.h"
#include "mir/geometry/overrides.h"
#include "mir/geometry/rectangles.h"

#include <memory>
#include <mutex>

namespace mir
{
class DisplayChanger;
namespace graphics
{
class DisplayConfiguration;
}
namespace input
{

class DisplayInputRegion : public InputRegion
{
public:
    DisplayInputRegion(mir::graphics::DisplayConfiguration const& initial_conf,
        std::shared_ptr<mir::DisplayChanger> const& display_changer);

    void override_orientation(uint32_t display_id, MirOrientation orientation) override;
    MirOrientation get_orientation(geometry::Point const& point) override;
    geometry::Rectangle bounding_rectangle();
    void confine(geometry::Point& point);

private:
    std::mutex rectangles_lock;
    mir::geometry::Overrides overrides;
    mir::geometry::Rectangles rectangles;

};

}
}

#endif /* MIR_INPUT_DISPLAY_INPUT_REGION_H_ */

