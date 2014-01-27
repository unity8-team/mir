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

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;

class mc::detail::OffscreenDisplayBuffer : public mg::DisplayBuffer
{
public:
    OffscreenDisplayBuffer()
        : rect(), old_fbo(), old_viewport(),
          fbo(), color_tex(), depth_rbo()
    {
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

    void setup(std::shared_ptr<mg::Buffer> const& new_buffer, geom::Rectangle const& new_rect)
    {
        if (fbo == 0)
        {
            glGenFramebuffers(1, &fbo);
            /* Set up color buffer... */
            glGenTextures(1, &color_tex);
        glBindTexture(GL_TEXTURE_2D, color_tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
            /* and depth buffer */
            glGenRenderbuffers(1, &depth_rbo);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        glBindTexture(GL_TEXTURE_2D, color_tex);
        new_buffer->bind_to_texture();
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, color_tex, 0);

        if (new_rect.size != rect.size)
        {
            std::cerr << "Updating for size " << new_rect.size << std::endl;
            /* depth buffer */
            glBindRenderbuffer(GL_RENDERBUFFER, depth_rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16,
                                  new_rect.size.width.as_uint32_t(),
                                  new_rect.size.height.as_uint32_t());
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, depth_rbo);

        }

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Failed to create FBO for buffer");

        rect = new_rect;
        buffer = new_buffer;
    }

    void make_current()
    {
        glGetIntegerv(GL_VIEWPORT, old_viewport);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &old_fbo);

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

private:
    void setup_fbo()
    {
    }

    std::shared_ptr<mg::Buffer> buffer;
    geom::Rectangle rect;
    GLint old_fbo;
    GLint old_viewport[4];
    GLuint fbo;
    GLuint color_tex;
    GLuint depth_rbo;
};

mc::CompositingScreenCapture::CompositingScreenCapture(
    std::unique_ptr<mg::GLContext> gl_context,
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mg::GraphicBufferAllocator> const& buffer_allocator,
    std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory)
    : gl_context{std::move(gl_context)},
      display{display},
      buffer_allocator{buffer_allocator},
      db_compositor_factory{db_compositor_factory},
      offscreen_display_buffer{new mc::detail::OffscreenDisplayBuffer{}}
{
}

mc::CompositingScreenCapture::~CompositingScreenCapture()
{
}

std::shared_ptr<mg::Buffer> mc::CompositingScreenCapture::buffer_for(
    mg::DisplayConfigurationOutputId output_id)
{
    auto extents = extents_for(output_id);

    mg::BufferProperties buffer_properties{
        extents.size,
        mir_pixel_format_xbgr_8888,
        mg::BufferUsage::hardware};

    auto scoped_gl_context =
        mir::raii::paired_calls([this] { gl_context->make_current(); },
                                [this] { gl_context->release_current(); });

    static std::vector<std::shared_ptr<mg::Buffer>> buffers;
    static int i = 0;
    static std::unique_ptr<mc::DisplayBufferCompositor> db_compositor;
    
    if (buffers.empty())
    {
        buffers.push_back(buffer_allocator->alloc_buffer(buffer_properties));
        buffers.push_back(buffer_allocator->alloc_buffer(buffer_properties));
        buffers.push_back(buffer_allocator->alloc_buffer(buffer_properties));

        db_compositor = db_compositor_factory->create_compositor_for(*offscreen_display_buffer);
    }

    auto ret_buf = buffers[i];
    offscreen_display_buffer->setup(ret_buf, extents);
    i = (i + 1) % buffers.size();

    db_compositor->composite();

    return ret_buf;
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
