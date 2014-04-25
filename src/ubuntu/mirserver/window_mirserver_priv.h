/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_UI_WINDOW_MIRSERVER_PRIV_H_
#define UBUNTU_APPLICATION_UI_WINDOW_MIRSERVER_PRIV_H_

#include <ubuntu/application/ui/window.h>

#include <EGL/egl.h>

#include <stddef.h>

#include <functional>
#include <memory>
#include <string>

namespace mir
{
namespace scene
{
class Surface;
}
namespace input
{
namespace receiver
{
class InputReceiverThread;
class InputPlatform;
}
}
namespace graphics
{
class InternalClient;
}
}

namespace ubuntu
{
namespace application
{
namespace mir
{
namespace server
{
class Instance;
class WindowProperties;

class Window
{
public:
    Window(Instance& instance, WindowProperties* properties,
           std::shared_ptr< ::mir::input::receiver::InputPlatform> const& platform,
           std::shared_ptr< ::mir::graphics::InternalClient> const& internal_client);
    ~Window();

    UAUiWindow* as_u_window();
    static Window* from_u_window(UAUiWindow* u_window);
    
    EGLNativeWindowType get_native_type();

protected:
    Window(Window const&) = delete;
    Window& operator=(Window const&) = delete;

private:
    std::unique_ptr<WindowProperties> window_properties;
    std::shared_ptr< ::mir::scene::Surface> surface;
    std::shared_ptr< ::mir::input::receiver::InputReceiverThread> input_thread;
    std::shared_ptr< ::mir::graphics::InternalClient> internal_client;
};
    
}
}
}
}

#endif // UBUNTU_APPLICATION_UI_WINDOW_MIRSERVER_PRIV_H_
