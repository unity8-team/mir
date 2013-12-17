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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/graphics/pixel_format_utils.h"

namespace mir
{
namespace graphics
{

bool contains_alpha( MirPixelFormat format )
{
    if (format == mir_pixel_format_abgr_8888
            || format == mir_pixel_format_argb_8888)
        return true;
    return false;
}

int32_t red_channel_depth( MirPixelFormat format )
{
    if( mir_pixel_format_invalid < format 
            && format < mir_pixel_formats )
        return 8;
    return 0;
}

int32_t blue_channel_depth( MirPixelFormat format )
{
    if( mir_pixel_format_invalid < format 
            && format < mir_pixel_formats )
        return 8;
    return 0;
}

int32_t green_channel_depth( MirPixelFormat format )
{
    if( mir_pixel_format_invalid < format 
            && format < mir_pixel_formats )
        return 8;
    return 0;
}

int32_t alpha_channel_depth( MirPixelFormat format )
{
    if (format == mir_pixel_format_abgr_8888
            || format == mir_pixel_format_argb_8888)
        return 8;
    return 0;
}

}
}
