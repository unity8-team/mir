/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_CLIENT_CURSOR_CONFIGURATION_H_
#define MIR_CLIENT_CURSOR_CONFIGURATION_H_

#include "mir/geometry/size.h"

#include <string>
#include <tuple>
#include <memory>

#include <stdint.h>

// Parameters for configuring the apperance and behavior of the system cursor. 
// Will grow to include cursors specified by raw RGBA data, hotspots, etc...
class MirCursorConfiguration 
{
public:
    MirCursorConfiguration(char const* name);
    MirCursorConfiguration(uint32_t const* pixels, mir::geometry::Size const& size);
    
    bool has_pixels() const;
    std::string cursor_name() const;
    std::tuple<uint32_t const*, mir::geometry::Size> pixels() const;
private:
    std::string const name;

    std::unique_ptr<uint32_t[]> pixel_data;
    mir::geometry::Size const pixels_size;
};

#endif // MIR_CLIENT_CURSOR_CONFIGURATION_H_
