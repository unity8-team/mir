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
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/buffer.h"
#include "mir/graphics/android/native_buffer.h"
#include "mir/graphics/android/sync_fence.h"
#include "swapping_gl_context.h"
#include "android_format_conversion-inl.h"
#include "fb_device.h"
#include "framebuffer_bundle.h"
#include "buffer.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg = mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

mga::FBDevice::FBDevice(
    std::shared_ptr<framebuffer_device_t> const& fbdev)
    : fb_device(fbdev)
{
    if (fb_device->setSwapInterval)
    {
        fb_device->setSwapInterval(fb_device.get(), 1);
    }

    mode(mir_power_mode_on);
}

void mga::FBDevice::post_gl(SwappingGLContext const& context)
{
    context.swap_buffers();
    auto const& buffer = context.last_rendered_buffer();
    auto native_buffer = buffer->native_buffer_handle();
    native_buffer->ensure_available_for(mga::BufferAccess::read);
    if (fb_device->post(fb_device.get(), native_buffer->handle()) != 0)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("error posting with fb device"));
    }
}

bool mga::FBDevice::post_overlays(
    SwappingGLContext const&, RenderableList const&, RenderableListCompositor const&)
{
    return false;
}

void mga::FBDevice::mode(MirPowerMode mode)
{
    int enable = 0;
    if (mode == mir_power_mode_on)
    {
        enable = 1;
    }
    
    if (fb_device->enableScreen)
    {
        fb_device->enableScreen(fb_device.get(), enable);
    }
}
