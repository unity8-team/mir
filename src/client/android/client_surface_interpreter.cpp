/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "client_surface_interpreter.h"
#include "../client_buffer.h"
#include <stdexcept>

namespace mcla=mir::client::android;

mcla::ClientSurfaceInterpreter::ClientSurfaceInterpreter(ClientSurface& surface)
 :  surface(surface),
    driver_pixel_format(-1)
{
}

ANativeWindowBuffer* mcla::ClientSurfaceInterpreter::driver_requests_buffer()
{
    auto buffer = surface.get_current_buffer();
    auto buffer_to_driver = buffer->get_native_handle();
    buffer_to_driver->format = driver_pixel_format;

    return buffer_to_driver;
}

static void empty(MirSurface * /*surface*/, void * /*client_context*/)
{}
void mcla::ClientSurfaceInterpreter::driver_returns_buffer(ANativeWindowBuffer*, int /*fence_fd*/)
{
    mir_wait_for(surface.next_buffer(empty, NULL));
}

void mcla::ClientSurfaceInterpreter::dispatch_driver_request_format(int format)
{
    driver_pixel_format = format;
}

int  mcla::ClientSurfaceInterpreter::driver_requests_info(int key) const
{
    switch (key)
    {
        case NATIVE_WINDOW_WIDTH:
        case NATIVE_WINDOW_DEFAULT_WIDTH:
            return surface.get_parameters().width;
        case NATIVE_WINDOW_HEIGHT:
        case NATIVE_WINDOW_DEFAULT_HEIGHT:
            return surface.get_parameters().height;
        case NATIVE_WINDOW_FORMAT:
            return driver_pixel_format;
        case NATIVE_WINDOW_TRANSFORM_HINT:
            return 0;
        default:
            throw std::runtime_error("driver requested unsupported query");
    }
}
