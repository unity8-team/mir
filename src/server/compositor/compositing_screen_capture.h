/*
 * Copyright © 2014 Canonical Ltd.
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

#ifndef MIR_COMPOSITOR_COMPOSITING_SCREEN_CAPTURE_H_
#define MIR_COMPOSITOR_COMPOSITING_SCREEN_CAPTURE_H_

#include "mir/graphics/display_configuration.h"

#include <memory>

namespace mir
{
namespace geometry { struct Rectangle; }
namespace graphics
{ 
class GLContext;
class Display;
class GraphicBufferAllocator;
class Buffer;
}

namespace compositor
{
class DisplayBufferCompositorFactory;
namespace detail { class OffscreenDisplayBuffer; }

class CompositingScreenCapture
{
public:
    CompositingScreenCapture(
        std::unique_ptr<graphics::GLContext> gl_context,
        std::shared_ptr<graphics::Display> const& display,
        std::shared_ptr<graphics::GraphicBufferAllocator> const& buffer_allocator,
        std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory);
    ~CompositingScreenCapture();

    std::shared_ptr<graphics::Buffer> buffer_for(
        graphics::DisplayConfigurationOutputId output_id);

private:
    geometry::Rectangle extents_for(graphics::DisplayConfigurationOutputId output_id);

    std::unique_ptr<graphics::GLContext> const gl_context;
    std::shared_ptr<graphics::Display> const display;
    std::shared_ptr<graphics::GraphicBufferAllocator> const buffer_allocator;
    std::shared_ptr<DisplayBufferCompositorFactory> const db_compositor_factory;
    std::unique_ptr<detail::OffscreenDisplayBuffer> const offscreen_display_buffer;
};

}
}

#endif /* MIR_COMPOSITOR_COMPOSITING_SCREEN_CAPTURE_H_ */
