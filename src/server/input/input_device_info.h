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

#ifndef MIR_INPUT_INPUT_DEVICE_INFO_H_
#define MIR_INPUT_INPUT_DEVICE_INFO_H_

#include <memory>
#include <string>

namespace mir
{
namespace input
{

struct InputDeviceIdentifier
{
    std::string name;
    std::string location;
    std::string unique_id;
    uint16_t bus;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;
};

class InputDeviceInfo
{
public:
    InputDeviceInfo() = default;
    enum DeviceClasses
    {
        cursor      = 1<<1,
        keyboard    = 1<<2,
        touchpad    = 1<<3,
        touchscreen = 1<<4,
        gamepad     = 1<<5,
        joystick    = 1<<6,
    };
    virtual std::string path() const = 0;
    virtual uint32_t device_classes() const = 0;
    virtual InputDeviceIdentifier id() const = 0;
    virtual ~InputDeviceInfo() = default;
private:
    InputDeviceInfo(InputDeviceInfo const&) = delete;
    InputDeviceInfo& operator=(InputDeviceInfo const&) = delete;
};

}
}

#endif
