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

#include "window_properties_mirserver_priv.h"

#include <string>

namespace uams = ubuntu::application::mir::server;
namespace ms = mir::scene;

uams::WindowProperties::WindowProperties()
    : parameters(ms::a_surface()),
      cb(nullptr),
      input_ctx(nullptr)
{
}

UAUiWindowProperties* uams::WindowProperties::as_u_window_properties()
{
    return static_cast<UAUiWindowProperties*>(this);
}

uams::WindowProperties* uams::WindowProperties::from_u_window_properties(UAUiWindowProperties* u_properties)
{
    return static_cast<uams::WindowProperties*>(u_properties);
}
    
void uams::WindowProperties::set_title(char const* title, size_t length)
{
    parameters = parameters.of_name(std::string(title, length));
}

void uams::WindowProperties::set_input_cb_and_ctx(UAUiWindowInputEventCb callback, void* ctx)
{
    cb = callback;
    input_ctx = ctx;
}

void uams::WindowProperties::set_dimensions(uint32_t width, uint32_t height)
{
    parameters = parameters.of_size(width, height);
}

void uams::WindowProperties::set_pixel_format(MirPixelFormat const& format)
{
    parameters = parameters.of_pixel_format(format);
}

ms::SurfaceCreationParameters const& uams::WindowProperties::surface_parameters() const
{
    return parameters;
}

UAUiWindowInputEventCb uams::WindowProperties::input_cb() const
{
    return cb;
}

void* uams::WindowProperties::input_context() const
{
    return input_ctx;
}
