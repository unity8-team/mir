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

#ifndef MIR_INPUT_EVDEV_DEVICE_INFO_H_
#define MIR_INPUT_EVDEV_DEVICE_INFO_H_

#include "input_device_info.h"
#include <linux/input.h>

namespace mir
{
namespace input
{

class EvdevDeviceInfo : public InputDeviceInfo
{
public:
    EvdevDeviceInfo(char const* devicepath);
    std::string path() const override;
    uint32_t device_classes() const override;
    InputDeviceIdentifier id() const override;
private:
    uint32_t evaluate_device_class() const;
    std::string device_path;
    uint32_t device_class;
    InputDeviceIdentifier identifier;
    uint8_t key_bit_mask[(KEY_MAX+1)/8];
    uint8_t abs_bit_mask[(ABS_MAX+1)/8];
    uint8_t rel_bit_mask[(REL_MAX+1)/8];
    uint8_t sw_bit_mask[(SW_MAX+1)/8];
    uint8_t led_bit_mask[(LED_MAX+1)/8];
    uint8_t ff_bit_mask[(FF_MAX+1)/8];
    uint8_t property_bit_mask[(INPUT_PROP_MAX+1)/8];
};

}
}

#endif
