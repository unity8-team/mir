/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "android_input_window_handle.h"
#include "android_input_application_handle.h"

#include "mir/input/input_channel.h"
#include "mir/input/surface.h"
#include "mir/geometry/displacement.h"

#include <androidfw/InputTransport.h>
#include <androidfw/Input.h>

#define GLM_FORCE_RADIANS
#include <glm/gtx/transform.hpp>

#include <limits.h>

namespace mi = mir::input;
namespace mia = mi::android;
namespace geom = mir::geometry;

namespace
{
struct WindowInfo : public droidinput::InputWindowInfo
{
    WindowInfo(std::shared_ptr<mi::Surface> const& surface)
        : surface(surface)
    {
    }

    bool touchableRegionContainsPoint(int32_t x, int32_t y) const override
    {
        return surface->contains(geom::Point{x, y});
    }

    glm::mat4 getScreenToLocalTransformation() const override
    {
        // TODO Still missing further affine transformations that are applied to the
        // whole screen i.e. if something other than a orthographic matrix used.
        glm::mat4 transformation_matrix = surface->inverse_transformation();
        geom::Size size = surface->size();
        geom::Point top_left = surface->top_left();
        auto center = glm::vec3{size.width.as_int(), size.height.as_int(), 0}/2.0f;
        auto surface_center = glm::vec3{top_left.x.as_int(), top_left.y.as_int(), 0} + center;

        return glm::translate(center) * transformation_matrix * glm::translate(-surface_center);
    }

    bool frameContainsPoint(int32_t x, int32_t y) const override
    {
        // TODO frame vs touchableRegion?
        // shall we care about the difference and test against the surface rectangle instead
        // of the input regions specified?
        return surface->contains(geom::Point{x,y});
    }

    std::shared_ptr<mi::Surface> const surface;
};
}

mia::InputWindowHandle::InputWindowHandle(droidinput::sp<droidinput::InputApplicationHandle> const& input_app_handle,
                                          std::shared_ptr<mi::InputChannel> const& channel,
                                          std::shared_ptr<mi::Surface> const& surface)
  : droidinput::InputWindowHandle(input_app_handle),
    input_channel(channel),
    surface(surface)
{
    updateInfo();
}

bool mia::InputWindowHandle::updateInfo()
{
    if (!mInfo)
    {
        mInfo = new WindowInfo(surface);

        // TODO: How can we avoid recreating the InputChannel which the InputChannelFactory has already created?
        mInfo->inputChannel = new droidinput::InputChannel(droidinput::String8("TODO: Name"),
                                                           input_channel->server_fd());
    }

    mInfo->name = droidinput::String8(surface->name().c_str());
    mInfo->layoutParamsFlags = droidinput::InputWindowInfo::FLAG_NOT_TOUCH_MODAL;
    mInfo->layoutParamsType = droidinput::InputWindowInfo::TYPE_APPLICATION;
    mInfo->visible = true;
    mInfo->canReceiveKeys = true;
    mInfo->hasFocus = true;
    mInfo->hasWallpaper = false;
    mInfo->paused = false;
    mInfo->dispatchingTimeout = INT_MAX;
    mInfo->ownerPid = 0;
    mInfo->ownerUid = 0;
    mInfo->inputFeatures = 0;

    // TODO: Set touchableRegion and layer for touch events.

    return true;
}
