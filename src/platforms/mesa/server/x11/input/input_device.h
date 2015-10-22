/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#ifndef MIR_INPUT_X_INPUT_DEVICE_H_
#define MIR_INPUT_X_INPUT_DEVICE_H_

#include "mir/input/input_device.h"
#include "mir/input/input_device_info.h"
#include "mir/optional_value.h"

namespace mir
{
namespace input
{

namespace X
{

class XInputDevice : public input::InputDevice
{
public:
    XInputDevice(InputDeviceInfo const& info);

    std::shared_ptr<dispatch::Dispatchable> dispatchable();
    void start(InputSink* destination, EventBuilder* builder) override;
    void stop() override;
    InputDeviceInfo get_device_info() override;

    mir::optional_value<PointerSettings> get_pointer_settings() const override;
    void apply_settings(PointerSettings const& settings) override;

    InputSink* sink{nullptr};
    EventBuilder* builder{nullptr};
private:
    InputDeviceInfo info;
};

}

}
}

#endif // MIR_INPUT_X_INPUT_DEVICE_H_