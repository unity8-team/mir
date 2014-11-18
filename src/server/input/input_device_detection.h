/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_INPUT_DEVICE_DETECTION_H_
#define MIR_INPUT_INPUT_DEVICE_DETECTION_H_

#include "mir/fd.h"

#include <memory>

struct libevdev;

namespace mir
{
namespace input
{

class InputDeviceDetection
{
public:
    explicit InputDeviceDetection(const char* devpath);
    bool has_touchpad() const;
    bool has_multi_touch_screen() const;
    bool has_joystick() const;
    int num_joystick_axes() const;
private:
    Fd input_device;
    std::unique_ptr<libevdev, void(*)(libevdev *)> evdev_ptr;
};

}
}

#endif
