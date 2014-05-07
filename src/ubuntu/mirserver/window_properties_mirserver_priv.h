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

#ifndef UBUNTU_APPLICATION_UI_WINDOW_PROPERTIES_MIRSERVER_PRIV_H_
#define UBUNTU_APPLICATION_UI_WINDOW_PROPERTIES_MIRSERVER_PRIV_H_

#include <ubuntu/application/ui/window.h>

#include <mir/scene/surface_creation_parameters.h>
#include <mir_toolkit/common.h>

namespace ubuntu
{
namespace application
{
namespace mir
{
namespace server
{

class WindowProperties
{
public:
    WindowProperties();
    ~WindowProperties() = default;

    UAUiWindowProperties* as_u_window_properties();
    static WindowProperties* from_u_window_properties(UAUiWindowProperties* u_properties);
    
    void set_title(char const* title, size_t length);
    void set_input_cb_and_ctx(UAUiWindowInputEventCb cb, void* ctx);
    void set_dimensions(uint32_t width, uint32_t height);
    void set_pixel_format( MirPixelFormat const& format);
    
    ::mir::scene::SurfaceCreationParameters const& surface_parameters() const;
    UAUiWindowInputEventCb input_cb() const;
    void* input_context() const;
    
protected:
    WindowProperties(WindowProperties const&) = delete;
    WindowProperties& operator=(WindowProperties const&) = delete;

private:
    ::mir::scene::SurfaceCreationParameters parameters;

    UAUiWindowInputEventCb cb;
    void *input_ctx;
};

}
}
}
}

#endif
