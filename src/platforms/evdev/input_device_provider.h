/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_EVDEV_INPUT_DEVICE_PROVIDER_H_
#define MIR_INPUT_EVDEV_INPUT_DEVICE_PROVIDER_H_

#include <memory>

namespace mir
{
namespace input
{
class InputDevice;

namespace evdev
{

enum class Priority : uint32_t
{
    unsupported = 0,
    supported = 100,
    best = 255,
};

class InputDeviceProvider
{
public:
    InputDeviceProvider() = default;
    virtual ~InputDeviceProvider() = default;

    virtual Priority probe_device(char const* node) const = 0;
    virtual std::unique_ptr<InputDevice> create_device(char const* node) const = 0;

protected:
    InputDeviceProvider(InputDeviceProvider const& cp) = delete;
    InputDeviceProvider& operator=(InputDeviceProvider const& cp) = delete;
};

}
}
} // namespace mir

#endif // MIR_INPUT_EVDEV_INPUT_DEVICE_PROVIDER_H_
