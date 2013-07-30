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

#ifndef MIR_GRAPHICS_GBM_UDEV_VIDEO_DEVICES_H_
#define MIR_GRAPHICS_GBM_UDEV_VIDEO_DEVICES_H_

#include "video_devices.h"

#include <libudev.h>

namespace mir
{

class MainLoop;

namespace graphics
{
namespace gbm
{

class UdevVideoDevices : public VideoDevices
{
public:
    UdevVideoDevices(udev* udev_ctx);

    void register_change_handler(
        MainLoop& main_loop,
        std::function<void()> const& change_handler);

private:
    udev* const udev_ctx;
};

}
}
}

#endif /* MIR_GRAPHICS_GBM_UDEV_VIDEO_DEVICES_H_ */
