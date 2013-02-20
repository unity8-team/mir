/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_SURFACE_H_
#define UBUNTU_APPLICATION_UI_SURFACE_H_

#include "ubuntu/platform/shared_ptr.h"

#include "ubuntu/application/ui/session.h"
#include "ubuntu/application/ui/input/listener.h"

#include <EGL/egl.h>

namespace ubuntu
{
namespace application
{
namespace ui
{
/**
 * A surface is a rectangular region on screen.
 */
class Surface : public ubuntu::platform::ReferenceCountedBase
{
public:
    typedef ubuntu::platform::shared_ptr<Surface> Ptr;

    // Default surface API
    /** Toggles the visibility of the surface. */
    virtual void set_visible(int id, bool visible) = 0;
    /** Adjusts the alpha channel of a surface. */
    virtual void set_alpha(float alpha) = 0;
    /** Moves the surface to the specified coordinates. */
    virtual void move_to(int x, int y) = 0;
    /** Resizes the surface to the specified width and height. */
    virtual void resize(int w, int h) = 0;

    /** Binds to EGL/GL rendering API. */
    virtual EGLNativeWindowType to_native_window_type() = 0;

protected:
    Surface(const input::Listener::Ptr& input_listener) : input_listener(input_listener) {}
    virtual ~Surface() {}

    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    const input::Listener::Ptr& registered_input_listener() const
    {
        return input_listener;
    }

private:
    input::Listener::Ptr input_listener;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SURFACE_H_
