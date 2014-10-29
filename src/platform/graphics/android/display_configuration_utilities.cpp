/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "display_configuration_utilities.h"
#include "android_format_conversion-inl.h"

#include "mir/graphics/display_configuration.h"
#include "mir/geometry/point.h"
#include "mir/geometry/size.h"

#include <hardware/gralloc.h>
#include <hardware/fb.h>

namespace mg = mir::graphics;
namespace mga = mg::android;
namespace geom = mir::geometry;
mg::DisplayConfigurationOutput mga::create_output_configuration_from_fb(framebuffer_device_t const& device)
{
    geometry::Size size_in_mm(
        25.4f * float(device.width) / float(device.xdpi),
        25.4f * float(device.height) / float(device.ydpi)
        );

    return mg::DisplayConfigurationOutput
    {
        mg::DisplayConfigurationOutputId{0},
        mg::DisplayConfigurationCardId{0},
        mg::DisplayConfigurationOutputType::lvds,
        { mga::to_mir_format(device.format) }, // query for more?
        { {geom::Size{device.width, device.height}, double(device.fps)}},
        0,
        size_in_mm,
        true,
        true,
        geom::Point{0,0}, // top left
        0,
        mga::to_mir_format(device.format),
        mir_power_mode_on,
        mir_orientation_normal
    };
}
