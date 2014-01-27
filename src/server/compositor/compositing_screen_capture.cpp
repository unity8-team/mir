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

#include "compositing_screen_capture.h"
#include "mir/graphics/gl_context.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "mir/graphics/buffer.h"
#include "mir/graphics/buffer_id.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "mir/geometry/rectangle.h"
#include "mir/raii.h"
#include "default_display_buffer_compositor.h"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <boost/throw_exception.hpp>
#include <iostream>
#include <algorithm>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;

namespace
{

class OffscreenDisplayBuffer : public mg::DisplayBuffer
{
public:
    OffscreenDisplayBuffer(geom::Rectangle const& rect)
        : rect(rect), old_fbo(), old_viewport(),
          fbo(), color_tex(), depth_rbo()
    {
        glGetIntegerv(GL_VIEWPORT, old_viewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        /* Set up color buffer... */
        glGenTextures(1, &color_tex);
        glBindTexture(GL_TEXTURE_2D, color_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        /* and depth buffer */
        glGenRenderbuffers(1, &depth_rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                              rect.size.width.as_uint32_t(),
                              rect.size.height.as_uint32_t());
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  GL_RENDERBUFFER, depth_rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Failed to create FBO for buffer");
    }

    ~OffscreenDisplayBuffer()
    {
        release_current();
        if (color_tex != 0)
            glDeleteTextures(1, &color_tex);
        if (depth_rbo != 0)
            glDeleteRenderbuffers(1, &depth_rbo);
        if (fbo != 0)
            glDeleteFramebuffers(1, &fbo);
    }

    geom::Rectangle view_area() const
    {
        return rect;
    }

    void make_current()
    {
        glBindTexture(GL_TEXTURE_2D, color_tex);
        buffer->bind_to_texture();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, color_tex, 0);

        geom::Size buf_size = buffer->size();

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, buf_size.width.as_uint32_t(), buf_size.height.as_uint32_t());
    }

    void release_current()
    {
        glBindFramebuffer(GL_FRAMEBUFFER, old_fbo);
        glViewport(old_viewport[0], old_viewport[1],
                   old_viewport[2], old_viewport[3]);
    }

    void post_update()
    {
        glFinish();
    }

    bool can_bypass() const { return false; }

    void render_and_post_update(
        std::list<mg::Renderable> const&,
        std::function<void(mg::Renderable const&)> const&)
    {
    }

    MirOrientation orientation() const
    {
        return mir_orientation_normal;
    }

    void set_buffer(std::shared_ptr<mg::Buffer> const& new_buffer)
    {
        buffer = new_buffer;
    }

private:
    std::shared_ptr<mg::Buffer> buffer;
    geom::Rectangle rect;
    GLint old_fbo;
    GLint old_viewport[4];
    GLuint fbo;
    GLuint color_tex;
    GLuint depth_rbo;
};

}

class mc::detail::AreaCapture
{
public:
    AreaCapture(
        geom::Rectangle const& extents,
        DisplayBufferCompositorFactory& db_compositor_factory)
        : extents_(extents), offscreen_display_buffer{extents_},
          db_compositor{db_compositor_factory.create_compositor_for(offscreen_display_buffer)}
    {
        std::cerr << "Creating area for " << extents << std::endl;
    }

    void capture_to(std::shared_ptr<mg::Buffer> const& buffer)
    {
        offscreen_display_buffer.set_buffer(buffer);
        db_compositor->composite();
    }

    geometry::Rectangle extents()
    {
        return extents_;
    }

private:
    geom::Rectangle const extents_;
    OffscreenDisplayBuffer offscreen_display_buffer;
    std::unique_ptr<DisplayBufferCompositor> const db_compositor;
};

mc::CompositingScreenCapture::CompositingScreenCapture(
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mg::GraphicBufferAllocator> const& buffer_allocator,
    std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory)
    : gl_context{display->create_gl_context()},
      display{display},
      buffer_allocator{buffer_allocator},
      db_compositor_factory{db_compositor_factory}
{
}

mc::CompositingScreenCapture::~CompositingScreenCapture()
{
}

std::vector<uint8_t> pixels;

std::shared_ptr<mg::Buffer> mc::CompositingScreenCapture::acquire_buffer_for(
    mg::DisplayConfigurationOutputId output_id)
{
    auto extents = extents_for(output_id);

    auto scoped_gl_context =
        mir::raii::paired_calls([this] { gl_context->make_current(); },
                                [this] { gl_context->release_current(); });

    auto area_capture = get_or_create_area_capture(extents);
    auto buffer = get_or_create_buffer(extents.size);

    area_capture->capture_to(buffer);

    return buffer;
}

std::shared_ptr<mc::detail::AreaCapture>
mc::CompositingScreenCapture::get_or_create_area_capture(
    geom::Rectangle const& extents)
{
    if (!area_capture || area_capture->extents() != extents)
    {
        area_capture =
            std::make_shared<mc::detail::AreaCapture>(
                extents, *db_compositor_factory);
    }

    return area_capture;
}

geom::Rectangle mc::CompositingScreenCapture::extents_for(
    mg::DisplayConfigurationOutputId output_id)
{
    auto conf = display->configuration();
    geom::Rectangle extents;
    
    conf->for_each_output(
        [&](mg::DisplayConfigurationOutput const& output)
        { 
            if (output.id == output_id &&
                output.connected && output.used &&
                output.current_mode_index < output.modes.size())
            {
                extents = geom::Rectangle{
                    output.top_left,
                    output.modes[output.current_mode_index].size};
            }
        });

    if (extents == geom::Rectangle())
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Invalid output for screen capture"));
    }

    return extents;
}

void mc::CompositingScreenCapture::release_buffer(mg::BufferID buf_id)
{
    auto buf_iter = std::find_if(
        used_buffers.begin(), used_buffers.end(),
        [&buf_id](std::shared_ptr<mg::Buffer> const& buffer)
        {
            return buffer->id() == buf_id;
        });

    if (buf_iter != free_buffers.end())
    {
        free_buffers.push_back(*buf_iter);
        used_buffers.erase(buf_iter);
    }
}

std::shared_ptr<mg::Buffer>
mc::CompositingScreenCapture::get_or_create_buffer(
    geom::Size const& size)
{
    auto buf_iter = std::find_if(
        free_buffers.begin(), free_buffers.end(),
        [&size](std::shared_ptr<mg::Buffer> const& buffer)
        {
            return buffer->size() == size;
        });

    if (buf_iter == free_buffers.end())
    {
        mg::BufferProperties buffer_properties{
            size,
            mir_pixel_format_argb_8888,
            mg::BufferUsage::hardware};

        std::cerr << "Creating buffer for " << size << std::endl;
        auto buffer = buffer_allocator->alloc_buffer(buffer_properties);
        used_buffers.push_back(buffer);
    }
    else
    {
        used_buffers.push_back(*buf_iter);
        free_buffers.erase(buf_iter);
    }

    return used_buffers.back();
}
