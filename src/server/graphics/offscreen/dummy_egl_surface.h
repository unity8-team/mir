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

#ifndef MIR_GRAPHICS_OFFSCREEN_DUMMY_EGL_SURFACE_H_
#define MIR_GRAPHICS_OFFSCREEN_DUMMY_EGL_SURFACE_H_

#include <EGL/egl.h>

namespace mir
{
namespace graphics
{
namespace offscreen
{

class DummyEGLSurface
{
public:
    DummyEGLSurface(EGLDisplay egl_display);
    ~DummyEGLSurface() noexcept;

    operator EGLSurface() const;
    EGLConfig config() const;

private:
    DummyEGLSurface(DummyEGLSurface const&) = delete;
    DummyEGLSurface& operator=(DummyEGLSurface const&) = delete;

    EGLDisplay const egl_display;
    bool const surfaceless;
    EGLConfig const egl_config;
    EGLSurface const egl_surface;
};

}
}
}

#endif /* MIR_GRAPHICS_OFFSCREEN_DUMMY_EGL_SURFACE_H_ */
