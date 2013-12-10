/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_GRAPHICS_FRAMEBUFFER_CONFIGURATION_H_
#define MIR_GRAPHICS_FRAMEBUFFER_CONFIGURATION_H_

#include "mir_toolkit/common.h"
#include <vector>

namespace mir
{
namespace graphics
{

/*
 * \brief Select a specific pixel format to be used for the framebuffer
 * 
 * Implement this interface to change the behavior of the servers frame 
 * buffer creation. 
 */
class OutputConfiguration 
{
public:
    virtual ~OutputConfiguration() = default;

    typedef std::vector<MirPixelFormat> pixel_format_array;

    virtual MirPixelFormat get_pixel_format(pixel_format_array const& availableFormats) = 0;

protected:
    OutputConfiguration() = default;
    OutputConfiguration(OutputConfiguration const& c) = delete;
    OutputConfiguration& operator=(OutputConfiguration const& c) = delete;
};

}
}

#endif
