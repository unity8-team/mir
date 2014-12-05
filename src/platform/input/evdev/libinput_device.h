/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_EVDEV_LIBINPUT_DEVICE_H_
#define MIR_INPUT_EVDEV_LIBINPUT_DEVICE_H_

#include "mir/input/input_device.h"

struct libinput_device;

namespace mir
{
namespace input
{
namespace evdev
{
class LibInputWrapper;

class LibInputDevice : public input::InputDevice
{
public:
    LibInputDevice(std::shared_ptr<LibInputWrapper> const& lib, char const* path);
    ~LibInputDevice();
    void enable_input_events(Multiplexer& registry, EventSink& sink) override;
    void disable_input_events(Multiplexer& registry) override;
private:
    std::string path;
    std::unique_ptr<::libinput_device,libinput_device*(*)(::libinput_device*)> dev;
    std::shared_ptr<LibInputWrapper> const lib;
};
}
}
}

#endif
