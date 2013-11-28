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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_HWC11_DEVICE_H_
#define MIR_GRAPHICS_ANDROID_HWC11_DEVICE_H_

#include "mir/geometry/pixel_format.h"
#include "hwc_common_device.h"
#include "hwc_layerlist.h"
#include <memory>

namespace mir
{
namespace graphics
{
class Buffer;

namespace android
{
class HWCVsyncCoordinator;
class SyncFileOps;
class SyncFence;

class HWC11Device : public HWCCommonDevice
{
public:
    HWC11Device(std::shared_ptr<hwc_composer_device_1> const& hwc_device,
                std::shared_ptr<HWCVsyncCoordinator> const& coordinator);

    void prepare_composition();
    void gpu_render(EGLDisplay dpy, EGLSurface sur); 
    void post(Buffer const& buffer);

private:
    LayerList layer_list;

    std::shared_ptr<SyncFence> last_display_fence;
    std::shared_ptr<SyncFileOps> const sync_ops;
    unsigned int primary_display_config;
    geometry::PixelFormat fb_format;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_HWC11_DEVICE_H_ */
