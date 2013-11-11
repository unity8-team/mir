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

#ifndef MIR_GRAPHICS_OFFSCREEN_DISPLAY_BUFFER_H_
#define MIR_GRAPHICS_OFFSCREEN_DISPLAY_BUFFER_H_

#include "mir/graphics/display_buffer.h"
#include "mir/geometry/rectangle.h"
#include "mir/graphics/egl_resources.h"

#include <EGL/egl.h>

namespace mir
{
namespace geometry { struct Size; }
namespace graphics
{
namespace offscreen
{

namespace detail
{

class GLFramebufferObject
{
public:
    GLFramebufferObject(geometry::Size const& size);
    ~GLFramebufferObject();
    void bind() const;
    void unbind() const;
private:
    unsigned int color_renderbuffer;
    unsigned int depth_renderbuffer;
    unsigned int fbo;
};

}

class DisplayBuffer : public graphics::DisplayBuffer
{
public:
    DisplayBuffer(
        EGLDisplay egl_display,
        EGLConfig egl_config,
        EGLContext shared_context,
        geometry::Rectangle const& area);

    geometry::Rectangle view_area() const;
    void make_current();
    void release_current();
    void post_update();

    bool can_bypass() const;

private:
    EGLDisplay const egl_display;
    EGLConfig const egl_config;
    EGLContextStore const egl_context;
    EGLSurfaceStore const egl_surface_dummy;
    detail::GLFramebufferObject const fbo;
    geometry::Rectangle const area;
};

}
}
}

#endif /* MIR_GRAPHICS_OFFSCREEN_DISPLAY_BUFFER_H_ */
