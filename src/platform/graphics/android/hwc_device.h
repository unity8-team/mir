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

#ifndef MIR_GRAPHICS_ANDROID_HWC_DEVICE_H_
#define MIR_GRAPHICS_ANDROID_HWC_DEVICE_H_

#include "mir_toolkit/common.h"
#include "mir/graphics/android/sync_fence.h"
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

class HwcDevice : public HWCCommonDevice
{
public:
    HwcDevice(std::shared_ptr<hwc_composer_device_1> const& hwc_device,
              std::shared_ptr<HWCVsyncCoordinator> const& coordinator,
              std::shared_ptr<SyncFileOps> const& sync_ops);

    virtual void render_gl(SwappingGLContext const& context);
    virtual void render_gl_and_overlays(
        SwappingGLContext const& context,
        std::list<std::shared_ptr<Renderable>> const& list,
        std::function<void(Renderable const&)> const& render_fn);
    void post(Buffer const& buffer);

private:
    void prepare(hwc_display_contents_1_t& display_list);

    FBTargetLayerList layer_list;

    std::shared_ptr<SyncFileOps> const sync_ops;
    static size_t const num_displays{3}; //primary, external, virtual
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_HWC_DEVICE_H_ */
