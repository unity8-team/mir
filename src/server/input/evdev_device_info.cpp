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

#include "evdev_device_info.h"

#include "mir/fd.h"

#include <boost/exception/errinfo_errno.hpp>
#include <boost/exception/errinfo_file_name.hpp>
#include <boost/throw_exception.hpp>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <cstring>
#include <system_error>

namespace mi = mir::input;


namespace
{
constexpr size_t index_of_bit(size_t bit)
{
    return (bit + 7) / 8;
}
inline bool get_bit(uint8_t const* array, size_t bit)
{
    return array[bit/8] & (1<<(bit%8));
}

inline size_t get_num_bits(uint8_t const* array, std::initializer_list<size_t> bits)
{
    size_t ret = 0;
    for( auto const bit : bits)
        ret += get_bit(array, bit);
    return ret;
}

void fill_identifier(int fd, mi::InputDeviceIdentifier &id)
{
    char buffer[80];
    if (ioctl(fd, EVIOCGNAME(sizeof buffer - 1), &buffer) < 1)
        BOOST_THROW_EXCEPTION(
            std::system_error(std::error_code(errno, std::system_category()),
                              "Failed to get device name"));

    buffer[sizeof(buffer) - 1] = '\0';
    id.name = buffer;

    input_id device_input_id;
    if (ioctl(fd, EVIOCGID, &device_input_id) < 0)
        BOOST_THROW_EXCEPTION(
            std::system_error(std::error_code(errno, std::system_category()),
                              "Failed to get device input id"));

    id.bus = device_input_id.bustype;
    id.product = device_input_id.product;
    id.vendor = device_input_id.vendor;
    id.version = device_input_id.version;

    if (ioctl(fd, EVIOCGPHYS(sizeof buffer - 1), &buffer))
    {
        // rarely supported, especially for builtin devices.
        buffer[sizeof(buffer) - 1] = '\0';
        id.location = buffer;
    }

    if (ioctl(fd, EVIOCGUNIQ(sizeof buffer - 1), &buffer)) // rarely supported, especially for builtin devices.
    {
        buffer[sizeof(buffer) - 1] = '\0';
        id.unique_id = buffer;
    }
}
}

mi::EvdevDeviceInfo::EvdevDeviceInfo(const char* devpath)
    : device_path(devpath)
{
    try
    {
        std::memset(key_bit_mask, 0, sizeof key_bit_mask);
        std::memset(abs_bit_mask, 0, sizeof abs_bit_mask);
        std::memset(rel_bit_mask, 0, sizeof rel_bit_mask);
        std::memset(sw_bit_mask, 0, sizeof sw_bit_mask);
        std::memset(led_bit_mask, 0, sizeof led_bit_mask);
        std::memset(ff_bit_mask, 0, sizeof ff_bit_mask);
        std::memset(property_bit_mask, 0, sizeof property_bit_mask);

        mir::Fd input_device(::open(devpath, O_RDONLY|O_NONBLOCK));
        if (input_device < 0)
            BOOST_THROW_EXCEPTION(
                std::system_error(std::error_code(errno, std::system_category()),
                                  "Failed to open input device"));

        // Figure out the kinds of events the device reports.
        auto const get_bitmask = [&](int bit, size_t size, uint8_t* buf) -> void
        {
            if(ioctl(input_device, EVIOCGBIT(bit, size), buf) < 1)
                BOOST_THROW_EXCEPTION(
                    std::system_error(std::error_code(errno, std::system_category()),
                                      "Failed to query input device"));
        };
        get_bitmask(EV_KEY, sizeof key_bit_mask, key_bit_mask);
        get_bitmask(EV_REL, sizeof rel_bit_mask, rel_bit_mask);
        get_bitmask(EV_ABS, sizeof abs_bit_mask, abs_bit_mask);
        get_bitmask(EV_SW, sizeof sw_bit_mask, sw_bit_mask);
        get_bitmask(EV_LED, sizeof led_bit_mask, led_bit_mask);
        get_bitmask(EV_FF, sizeof ff_bit_mask, ff_bit_mask);

        if (ioctl(input_device, EVIOCGPROP(sizeof property_bit_mask), property_bit_mask) < 1)
            BOOST_THROW_EXCEPTION(
                std::system_error(std::error_code(errno, std::system_category()),
                                  "Failed to query devices properties"));

        fill_identifier(input_device, identifier);

        device_class = evaluate_device_class();
    }
    catch (boost::exception& e)
    {
        e << boost::errinfo_file_name(devpath);
        throw;
    }
}

std::string mi::EvdevDeviceInfo::path() const
{
    return device_path;
}

uint32_t mi::EvdevDeviceInfo::device_classes() const
{
    return device_class;
}

mi::InputDeviceIdentifier mi::EvdevDeviceInfo::id() const
{
    return identifier;
}

uint32_t mi::EvdevDeviceInfo::evaluate_device_class() const
{
    uint32_t classes = 0;
    auto const contains_non_zero = [](uint8_t const* array, int first, int last) -> bool
    {
        return std::any_of(array + first, array + last, [](uint8_t item) { return item!=0;});
    };

    bool const has_keys = contains_non_zero(key_bit_mask, 0, index_of_bit(BTN_MISC))
        || contains_non_zero(key_bit_mask, index_of_bit(KEY_OK), sizeof key_bit_mask);
    bool const has_gamepad_buttons =
        contains_non_zero(key_bit_mask, index_of_bit(BTN_MISC), index_of_bit(BTN_MOUSE))
        || contains_non_zero(key_bit_mask, index_of_bit(BTN_JOYSTICK), index_of_bit(BTN_DIGI));

    if (has_keys || has_gamepad_buttons)
        classes |= keyboard;

    if (get_bit(key_bit_mask, BTN_MOUSE) && get_bit(rel_bit_mask, REL_X) && get_bit(rel_bit_mask, REL_Y))
        classes |= cursor;

    bool const has_coordinates = get_bit(abs_bit_mask, ABS_X) &&
        get_bit(abs_bit_mask, ABS_Y);
    bool const has_mt_coordinates = get_bit(abs_bit_mask, ABS_MT_POSITION_X) &&
        get_bit(abs_bit_mask, ABS_MT_POSITION_Y);
    bool const is_direct = get_bit(property_bit_mask, INPUT_PROP_DIRECT);
    bool const finger_but_no_pen = get_bit(key_bit_mask, BTN_TOOL_FINGER)
        && !get_bit(key_bit_mask, BTN_TOOL_PEN);
    bool const has_touch = get_bit(key_bit_mask, BTN_TOUCH);

    if (finger_but_no_pen && !is_direct && (has_coordinates|| has_mt_coordinates))
        classes |= touchpad;
    else if (has_touch && ((has_mt_coordinates && !has_gamepad_buttons)
                           || has_coordinates))
        classes |= touchscreen;


    bool const has_joystick_axis = 0 < get_num_bits(
        abs_bit_mask, {ABS_Z,
        ABS_RX, ABS_RY, ABS_RZ,
        ABS_THROTTLE, ABS_RUDDER, ABS_WHEEL, ABS_GAS, ABS_BRAKE,
        ABS_HAT0X, ABS_HAT0Y, ABS_HAT1X, ABS_HAT1Y, ABS_HAT2X, ABS_HAT2Y, ABS_HAT3X, ABS_HAT3Y,
        ABS_TILT_X, ABS_TILT_Y
        });

    if (has_joystick_axis || (!has_touch && has_coordinates))
        classes |= joystick;

    if (has_gamepad_buttons)
        classes |= gamepad;

    return classes;
}
