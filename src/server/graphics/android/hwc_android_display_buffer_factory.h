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

#ifndef MIR_GRAPHICS_ANDROID_HWC_ANDROID_DISPLAY_BUFFER_FACTORY_H_
#define MIR_GRAPHICS_ANDROID_HWC_ANDROID_DISPLAY_BUFFER_FACTORY_H_

#include "android_display_buffer_factory.h"

namespace mir
{
namespace graphics
{
namespace android
{

class HWCDevice;

class HWCAndroidDisplayBufferFactory : public AndroidDisplayBufferFactory
{
public:
    HWCAndroidDisplayBufferFactory(std::shared_ptr<HWCDevice> const& hwc_device);

    std::unique_ptr<DisplayBuffer> create_display_buffer(
        std::shared_ptr<AndroidFramebufferWindowQuery> const& native_win,
        EGLDisplay egl_display,
        EGLContext egl_context_shared);

private:
    std::shared_ptr<HWCDevice> const hwc_device;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_HWC_ANDROID_DISPLAY_BUFFER_FACTORY_H_ */
