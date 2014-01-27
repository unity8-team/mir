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

#include "mir/frontend/screen_capture.h"

#include <memory>
#include <vector>

namespace mir
{
namespace geometry { struct Rectangle; }
namespace graphics
{ 
class GLContext;
class Display;
class GraphicBufferAllocator;
}

namespace compositor
{
class DisplayBufferCompositorFactory;
namespace detail { class AreaCapture; }

class CompositingScreenCapture : public frontend::ScreenCapture
{
public:
    CompositingScreenCapture(
        std::shared_ptr<graphics::Display> const& display,
        std::shared_ptr<graphics::GraphicBufferAllocator> const& buffer_allocator,
        std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory);
    ~CompositingScreenCapture();

    std::shared_ptr<graphics::Buffer> acquire_buffer_for(
        graphics::DisplayConfigurationOutputId output_id);

    void release_buffer(graphics::BufferID buf_id);

private:
    geometry::Rectangle extents_for(graphics::DisplayConfigurationOutputId output_id);
    std::shared_ptr<detail::AreaCapture> get_or_create_area_capture(geometry::Rectangle const& rect);
    std::shared_ptr<graphics::Buffer> get_or_create_buffer(geometry::Size const& size);

    std::unique_ptr<graphics::GLContext> const gl_context;
    std::shared_ptr<graphics::Display> const display;
    std::shared_ptr<graphics::GraphicBufferAllocator> const buffer_allocator;
    std::shared_ptr<DisplayBufferCompositorFactory> const db_compositor_factory;

    std::shared_ptr<detail::AreaCapture> area_capture;
    std::vector<std::shared_ptr<graphics::Buffer>> free_buffers;
    std::vector<std::shared_ptr<graphics::Buffer>> used_buffers;
};

}
}

#endif /* MIR_COMPOSITOR_COMPOSITING_SCREEN_CAPTURE_H_ */
