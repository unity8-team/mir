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
#include "mir/geometry/transformation.h"

#include <androidfw/InputTransport.h>
#include <androidfw/Input.h>

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

    std::function<void (android::PointerCoords *,size_t)> getScreenToLocalTransformation() const override
    {
        geom::Transformation const& transformation{ surface->get_transformation() };

        // selecting a transformation short cut:
        if (transformation.is_identity())
        {
            return [](android::PointerCoords *, size_t) {};
        }
        if (transformation.is_translation())
        {
            geom::Displacement movement{transformation.get_translation()};
            return [movement](android::PointerCoords * coords, size_t count)
            {
                for (size_t i = 0;i != count; ++i)
                {
                    geom::Point pos{coords[i].getAxisValue(AMOTION_EVENT_AXIS_X),
                        coords[i].getAxisValue(AMOTION_EVENT_AXIS_Y) };
                    pos = pos - movement;
                    coords[i].setAxisValue(AMOTION_EVENT_AXIS_X, pos.x.as_float());
                    coords[i].setAxisValue(AMOTION_EVENT_AXIS_Y, pos.y.as_float());
                }
            };
        }
        if (transformation.is_scaling())
        {
            float scale{transformation.get_scale()};
            return [scale](android::PointerCoords * coords, size_t count)
            {
                for (size_t i = 0;i != count; ++i)
                {
                    float pos_x{coords[i].getAxisValue(AMOTION_EVENT_AXIS_X)};
                    float pos_y{coords[i].getAxisValue(AMOTION_EVENT_AXIS_Y)};
                    pos_x = pos_x/scale;
                    pos_y = pos_y/scale;
                    coords[i].setAxisValue(AMOTION_EVENT_AXIS_X, pos_x);
                    coords[i].setAxisValue(AMOTION_EVENT_AXIS_Y, pos_y);
                }
            };
        }
        if (transformation.is_scaling_translation())
        {
            geom::Displacement movement{transformation.get_translation()};
            float movement_x = movement.dx.as_float();
            float movement_y = movement.dx.as_float();
            float scale{transformation.get_scale()};
            return [=](android::PointerCoords * coords, size_t count)
            {
                for (size_t i = 0;i != count; ++i)
                {
                    float pos_x{coords[i].getAxisValue(AMOTION_EVENT_AXIS_X)};
                    float pos_y{coords[i].getAxisValue(AMOTION_EVENT_AXIS_Y)};
                    pos_x = (pos_x - movement_x)/scale;
                    pos_y = (pos_y - movement_y)/scale;
                    coords[i].setAxisValue(AMOTION_EVENT_AXIS_X, pos_x);
                    coords[i].setAxisValue(AMOTION_EVENT_AXIS_Y, pos_y);
                }
            };
        }

        glm::mat4 transformation_matrix(transformation.get_inverse_matrix());
        return [=](android::PointerCoords * coords, size_t count)
        {
            for (size_t i = 0;i != count; ++i)
            {
                glm::vec4 pos
                {
                    coords[i].getAxisValue(AMOTION_EVENT_AXIS_X),
                    coords[i].getAxisValue(AMOTION_EVENT_AXIS_Y),
                    0.0,
                    1.0,
                };
                pos = transformation_matrix*pos;
                coords[i].setAxisValue(AMOTION_EVENT_AXIS_X, pos.x);
                coords[i].setAxisValue(AMOTION_EVENT_AXIS_Y, pos.y);
            }
        };
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
