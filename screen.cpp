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
    m_depth = 32; // FIXME: assumed, could use "bytes_per_pixel" to calculate it

    // Mode = Resolution & refresh rate
    mir::graphics::DisplayConfigurationMode mode = screen.modes.at(screen.current_mode_index);
    m_geometry.setWidth(mode.size.width.as_int());
    m_geometry.setHeight(mode.size.height.as_int());

    m_refreshRate = mode.vrefresh_hz;
}
