/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_GRAPHICS_DEFAULT_OUTPUT_CONFIGURATION_H_
#define MIR_GRAPHICS_DEFAULT_OUTPUT_CONFIGURATION_H_

#include "mir/graphics/output_configuration.h"

namespace mir
{
namespace graphics
{

/*!
 * \brief This implementation selects an opaque pixel format if possible.
 */
class DefaultOutputConfiguration : public OutputConfiguration
{
public:
    MirPixelFormat get_pixel_format(pixel_format_array const& availableFormats) override;
};

}
}

#endif

