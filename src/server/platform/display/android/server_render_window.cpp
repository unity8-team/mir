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
 * Authored by:
 *   Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/compositor/buffer.h"
#include "server_render_window.h"
#include "display_support_provider.h"
#include "fb_swapper.h"
#include "../../graphics_buffer/android/android_buffer.h"
#include "../../graphics_buffer/android/android_format_conversion-inl.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>


#include <thread>
#include <chrono>
namespace mc=mir::compositor;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

mga::ServerRenderWindow::ServerRenderWindow(std::shared_ptr<mga::FBSwapper> const& swapper,
                                            std::shared_ptr<mga::DisplaySupportProvider> const& display_poster)
    : swapper(swapper),
      poster(display_poster),
      format(mga::to_android_format(poster->display_format()))
{
}

ANativeWindowBuffer* mga::ServerRenderWindow::driver_requests_buffer()
{
    auto buffer = swapper->compositor_acquire();
    auto handle = buffer->native_buffer_handle().get();
    buffers_in_driver[handle] = buffer;

    return handle;
}

//sync object could be passed to hwc. we don't need to that yet though
void mga::ServerRenderWindow::driver_returns_buffer(ANativeWindowBuffer* returned_handle, std::shared_ptr<SyncObject> const&)
{
    auto buffer_it = buffers_in_driver.find(returned_handle); 
    if (buffer_it == buffers_in_driver.end())
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("driver is returning buffers it never was given!"));
    }

    auto buffer = buffer_it->second;
    buffers_in_driver.erase(buffer_it);
    poster->set_next_frontbuffer(buffer);
    swapper->compositor_release(buffer);
}

void mga::ServerRenderWindow::dispatch_driver_request_format(int request_format)
{
    format = request_format;
}

int mga::ServerRenderWindow::driver_requests_info(int key) const
{
    geom::Size size;
    switch(key)
    {
        case NATIVE_WINDOW_DEFAULT_WIDTH:
        case NATIVE_WINDOW_WIDTH:
            size = poster->display_size();
            return size.width.as_uint32_t();
        case NATIVE_WINDOW_DEFAULT_HEIGHT:
        case NATIVE_WINDOW_HEIGHT:
            size = poster->display_size();
            return size.height.as_uint32_t();
        case NATIVE_WINDOW_FORMAT:
            return format;
        case NATIVE_WINDOW_TRANSFORM_HINT:
            return 0; 
        default:
            BOOST_THROW_EXCEPTION(std::runtime_error("driver requests info we dont provide. key: " + key));
    }
}
