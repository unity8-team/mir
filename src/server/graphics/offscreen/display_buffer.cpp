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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "display_buffer.h"
#include "gl_extensions_base.h"

#include <boost/throw_exception.hpp>

#include <cstring>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <vector>
#include <fstream>
#include <sstream>

namespace mg = mir::graphics;
namespace mgo = mg::offscreen;
namespace geom = mir::geometry;

namespace
{

EGLint const default_egl_context_attr[] =
{
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
};

class GLExtensions : public mgo::GLExtensionsBase
{
public:
    GLExtensions() :
        mgo::GLExtensionsBase{
            reinterpret_cast<char const*>(glGetString(GL_EXTENSIONS))}
    {
    }
};
    std::vector<char> pixels;

void take_screenshot(std::string const& path, geom::Size const& size)
{
    auto const w = size.width.as_uint32_t();
    auto const h = size.height.as_uint32_t();
    pixels.resize(4*h*w);

    //glReadPixels(0, 0, w, h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels.data());

    for (uint32_t i = 0; i < h; i++)
    {
        glReadPixels(0, i, w, 1, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
                     &pixels[(h - i - 1) * w * 4]);
    }

    std::ofstream output(path, std::ios::out | std::ios::binary);
    output.write(pixels.data(), pixels.size());
}

}

mgo::detail::GLFramebufferObject::GLFramebufferObject(geom::Size const& size)
    : size{size}, color_renderbuffer{0}, depth_renderbuffer{0}, fbo{0}
{
    GLExtensions const extensions;

    GLenum gl_color_format{GL_RGBA4};
    GLenum const gl_depth_format{GL_DEPTH_COMPONENT16};

    if (extensions.support("GL_ARM_rgba8") ||
        extensions.support("GL_OES_rgb8_rgba8"))
    {
        gl_color_format = GL_RGBA8_OES;
    }

    glGenRenderbuffers(1, &color_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, color_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, gl_color_format,
                          size.width.as_int(), size.height.as_int());

    /* Create a renderbuffer for the depth attachment */
    glGenRenderbuffers(1, &depth_renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depth_renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, gl_depth_format,
                          size.width.as_int(), size.height.as_int());

    /* Create a FBO and set it up */
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                              GL_RENDERBUFFER, color_renderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_renderbuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

mgo::detail::GLFramebufferObject::~GLFramebufferObject()
{
    if (fbo)
    {
        glDeleteFramebuffers(1, &fbo);
        fbo = 0;
    }
    if (color_renderbuffer)
    {
        glDeleteRenderbuffers(1, &color_renderbuffer);
        color_renderbuffer = 0;
    }
    if (depth_renderbuffer)
    {
        glDeleteRenderbuffers(1, &depth_renderbuffer);
        depth_renderbuffer = 0;
    }
}

void mgo::detail::GLFramebufferObject::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, size.width.as_int(), size.height.as_int());
}

void mgo::detail::GLFramebufferObject::unbind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

mgo::DisplayBuffer::DisplayBuffer(
    EGLDisplay egl_display,
    EGLContext shared_context,
    geom::Rectangle const& area)
    : egl_display{egl_display},
      dummy_egl_surface{egl_display},
      egl_context{egl_display,
                  eglCreateContext(egl_display, dummy_egl_surface.config(),
                                   shared_context, default_egl_context_attr)},
      fbo{area.size},
      area(area)
{
}

geom::Rectangle mgo::DisplayBuffer::view_area() const
{
    return area;
}

void mgo::DisplayBuffer::make_current()
{
    if (eglGetCurrentContext() == egl_context)
        return;

    if (eglMakeCurrent(egl_display, dummy_egl_surface, dummy_egl_surface,
                       egl_context) != EGL_TRUE)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to make EGL surface current.\n"));
    }

    fbo.bind();
}

void mgo::DisplayBuffer::release_current()
{
    fbo.unbind();
    eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
}

void mgo::DisplayBuffer::post_update()
{
    glFinish();

    /* Test code
    */
    static int count = 0;
    ++count;
    //if (count % 1000 == 0)
    {
        std::stringstream ss;
        ss << "/tmp/";
        ss.width(5);
        ss.fill('0');
        ss <<  count;
        ss.width(0);
        ss << ".rgba";
        take_screenshot(ss.str(), area.size);
    }
    //
}

bool mgo::DisplayBuffer::can_bypass() const
{
    return false;
}
