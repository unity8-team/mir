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
 *
 * Author: Gerry Boland <gerry.boland@canonical.com>
 */

#include "screen.h"

#include "mir/geometry/size.h"

namespace mg = mir::geometry;

Screen::Screen(mir::graphics::DisplayConfigurationOutput const &screen)
{
    readMirDisplayConfiguration(screen);
}

void Screen::readMirDisplayConfiguration(mir::graphics::DisplayConfigurationOutput const &screen)
{
    // Physical screen size
    m_physicalSize.setWidth(screen.physical_size_mm.width.as_float());
    m_physicalSize.setHeight(screen.physical_size_mm.height.as_float());

    // Pixel Format
    mg::PixelFormat pixelFormat = screen.pixel_formats.at(screen.current_format_index);
    switch(pixelFormat) {
    case mg::PixelFormat::argb_8888:
        m_format = QImage::Format_ARGB32;
        break;
    case mg::PixelFormat::xrgb_8888:
        m_format = QImage::Format_ARGB32_Premultiplied;
        break;
        // Don't think Qt supports any others (abgr_8888, xbgr_8888, bgr_888)
    default:
        m_format = QImage::Format_Invalid;
        break;
    }

    // Pixel depth
    m_depth = 8 * mg::bytes_per_pixel(pixelFormat);

    // Mode = Resolution & refresh rate
    mir::graphics::DisplayConfigurationMode mode = screen.modes.at(screen.current_mode_index);
    m_geometry.setWidth(mode.size.width.as_int());
    m_geometry.setHeight(mode.size.height.as_int());

    m_refreshRate = 60; //mode.vrefresh_hz value incorrect;
}
