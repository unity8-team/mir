/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_LIBINPUT_H_
#define MIR_TEST_DOUBLES_MOCK_LIBINPUT_H_

#include <gmock/gmock.h>

#include <libinput.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockLibInput
{
public:
    MockLibInput();
    ~MockLibInput() noexcept;

    MOCK_METHOD1(libinput_ref, libinput*(libinput*));
    MOCK_METHOD1(libinput_unref, libinput*(libinput*));
    MOCK_METHOD1(libinput_dispatch, int(libinput*));
    MOCK_METHOD1(libinput_get_fd, int(libinput*));
    MOCK_METHOD1(libinput_get_event, libinput_event*(libinput*));
    MOCK_METHOD1(libinput_event_get_type, libinput_event_type(libinput_event*));
    MOCK_METHOD1(libinput_event_destroy, void(libinput_event*));
    MOCK_METHOD1(libinput_event_get_device, libinput_device*(libinput_event*));
    MOCK_METHOD1(libinput_event_get_pointer_event, libinput_event_pointer*(libinput_event*));
    MOCK_METHOD1(libinput_event_get_keyboard_event, libinput_event_keyboard*(libinput_event*));
    MOCK_METHOD1(libinput_event_get_touch_event, libinput_event_touch*(libinput_event*));

    MOCK_METHOD1(libinput_event_keyboard_get_time, uint32_t(libinput_event_keyboard*));
    MOCK_METHOD1(libinput_event_keyboard_get_key, uint32_t(libinput_event_keyboard*));
    MOCK_METHOD1(libinput_event_keyboard_get_key_state, libinput_key_state(libinput_event_keyboard*));
    MOCK_METHOD1(libinput_event_keyboard_get_seat_key_count, uint32_t(libinput_event_keyboard*));

    MOCK_METHOD1(libinput_event_pointer_get_time, uint32_t(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_dx, double(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_dy, double(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_absolute_x, double(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_absolute_y, double(libinput_event_pointer*));
    MOCK_METHOD2(libinput_event_pointer_get_absolute_x_transformed, double(libinput_event_pointer*, uint32_t));
    MOCK_METHOD2(libinput_event_pointer_get_absolute_y_transformed, double(libinput_event_pointer*, uint32_t));
    MOCK_METHOD1(libinput_event_pointer_get_button, uint32_t(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_button_state, libinput_button_state(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_seat_button_count, uint32_t(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_axis, libinput_pointer_axis(libinput_event_pointer*));
    MOCK_METHOD1(libinput_event_pointer_get_axis_value, double(libinput_event_pointer*));

    MOCK_METHOD1(libinput_event_touch_get_time, uint32_t(libinput_event_touch*));
    MOCK_METHOD1(libinput_event_touch_get_slot, int32_t(libinput_event_touch*));
    MOCK_METHOD1(libinput_event_touch_get_seat_slot, int32_t(libinput_event_touch*));
    MOCK_METHOD1(libinput_event_touch_get_x, double(libinput_event_touch*));
    MOCK_METHOD1(libinput_event_touch_get_y, double(libinput_event_touch*));
    MOCK_METHOD2(libinput_event_touch_get_x_transformed, double(libinput_event_touch*, uint32_t));
    MOCK_METHOD2(libinput_event_touch_get_y_transformed, double(libinput_event_touch*, uint32_t));

    MOCK_METHOD2(libinput_path_create_context, libinput*(const libinput_interface *, void*));
    MOCK_METHOD2(libinput_path_add_device, libinput_device*(const libinput*, const char*));
    MOCK_METHOD1(libinput_path_remove_device, void(libinput_device*));
    MOCK_METHOD1(libinput_device_unref, libinput_device*(libinput_device*));
    MOCK_METHOD1(libinput_device_ref, libinput_device*(libinput_device*));
};

}
}
}

#endif
