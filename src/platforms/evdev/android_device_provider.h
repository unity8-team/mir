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

#ifndef MIR_INPUT_EVDEV_ANDROID_DEVICE_PROVIDER_H_
#define MIR_INPUT_EVDEV_ANDROID_DEVICE_PROVIDER_H_

#include "input_device_provider.h"

namespace mir
{
namespace input
{
namespace evdev
{

class AndroidDeviceProvider : public InputDeviceProvider
{
public:
    Priority probe_device(char const* path) const override;
    std::unique_ptr<InputDevice> create_device(char const* path) const override;
};

}
}
}

#endif // MIR_INPUT_EVDEV_ANDROID_DEVICE_PROVIDER_H_
