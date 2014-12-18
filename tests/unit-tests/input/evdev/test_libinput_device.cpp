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

#include "src/platforms/evdev/libinput_device.h"
#include "src/platforms/evdev/libinput_wrapper.h"

#include "mir/input/input_event_handler_register.h"
#include "mir/input/input_device_registry.h"
#include "mir/input/event_sink.h"
#include "mir_test_doubles/mock_libinput.h"
#include "mir_test_doubles/mock_input_event_handler_register.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mie = mi::evdev;
namespace mtd = mir::test::doubles;

namespace
{

class StubInputDeviceRegistry : public mi::InputDeviceRegistry
{
public:
    void add_device(std::shared_ptr<mi::InputDevice> const&) override {}
    void remove_device(std::shared_ptr<mi::InputDevice> const&) override {}
};

class MockEventSink : public mi::EventSink
{
public:
    MOCK_METHOD1(handle_input,void(MirEvent const& event));
};

struct LibInputDevice : public ::testing::Test
{
    ::testing::NiceMock<mir::test::doubles::MockLibInput> mock_libinput;
    ::testing::NiceMock<mtd::MockInputEventHandlerRegister> mock_registry;
    ::testing::NiceMock<MockEventSink> mock_sink;
    std::shared_ptr<mie::LibInputWrapper> wrapper;

    libinput* fake_input = reinterpret_cast<libinput*>(0xF4C3);
    libinput_device* fake_device = reinterpret_cast<libinput_device*>(0xF4C4);
    libinput_event* fake_event = reinterpret_cast<libinput_event*>(0xF4C5);
    libinput_event_keyboard* fake_keyboard_event = reinterpret_cast<libinput_event_keyboard*>(0xF46C);
    libinput_event_pointer* fake_pointer_event = reinterpret_cast<libinput_event_pointer*>(0xF4C6);
    libinput_event_touch* fake_touch_event = reinterpret_cast<libinput_event_touch*>(0xF4C7);

    LibInputDevice()
    {
        using namespace ::testing;
        ON_CALL(mock_libinput, libinput_path_create_context(_,_))
            .WillByDefault(Return(fake_input));
        ON_CALL(mock_libinput, libinput_path_add_device(fake_input,_))
            .WillByDefault(Return(fake_device));
        ON_CALL(mock_libinput, libinput_device_ref(fake_device))
            .WillByDefault(Return(fake_device));
        ON_CALL(mock_libinput, libinput_device_unref(fake_device))
            .WillByDefault(Return(nullptr));

        wrapper = std::make_shared<mie::LibInputWrapper>();
    }
};

}

TEST_F(LibInputDevice, start_creates_and_unrefs_libinput_device_from_path)
{
    using namespace ::testing;
    // according to manual libinput_path_add_device creates a temporary device with a ref count 0.
    // hence it needs a manual ref call
    char const* path = "/path/to/dev";
    EXPECT_CALL(mock_libinput, libinput_path_add_device(fake_input,StrEq(path)))
        .Times(1);
    EXPECT_CALL(mock_libinput, libinput_device_ref(fake_device))
        .Times(1);
    mie::LibInputDevice dev(wrapper, path);
    dev.start(mock_registry, mock_sink);
}

TEST_F(LibInputDevice, stop_unrefs_libinput_device)
{
    using namespace ::testing;
    char const* path = "/path/to/dev";
    EXPECT_CALL(mock_libinput, libinput_device_unref(fake_device))
        .Times(1);
    mie::LibInputDevice dev(wrapper, path);
    dev.start(mock_registry, mock_sink);
    dev.stop(mock_registry);
}

TEST_F(LibInputDevice, process_event_converts_pointer_event)
{
    using namespace ::testing;
    mie::LibInputDevice dev(wrapper, "dev");

    MirEvent mir_event;
    uint32_t event_time = 14;
    float x = 15;
    float y = 17;

    EXPECT_CALL(mock_libinput, libinput_event_get_type(fake_event))
        .WillOnce(Return(LIBINPUT_EVENT_POINTER_MOTION));
    EXPECT_CALL(mock_libinput, libinput_event_get_pointer_event(fake_event))
        .WillOnce(Return(fake_pointer_event));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_time(fake_pointer_event))
        .WillOnce(Return(event_time));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_dx(fake_pointer_event))
        .WillOnce(Return(x));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_dy(fake_pointer_event))
        .WillOnce(Return(y));
    EXPECT_CALL(mock_sink, handle_input(_))
        .WillOnce(SaveArg<0>(&mir_event));

    dev.start(mock_registry, mock_sink);
    dev.process_event(fake_event);

    EXPECT_THAT(mir_event.type, Eq(mir_event_type_motion));
    EXPECT_THAT(mir_event.motion.event_time, Eq(event_time));
    EXPECT_THAT(mir_event.motion.pointer_count, Eq(1));
    EXPECT_THAT(mir_event.motion.button_state, Eq(MirMotionButton(0)));
    EXPECT_THAT(mir_event.motion.pointer_coordinates[0].tool_type, Eq(mir_motion_tool_type_mouse));
    EXPECT_THAT(mir_event.motion.pointer_coordinates[0].x, Eq(x));
    EXPECT_THAT(mir_event.motion.pointer_coordinates[0].y, Eq(y));
}

TEST_F(LibInputDevice, process_event_accumulates_pointer_movement)
{
    using namespace ::testing;
    mie::LibInputDevice dev(wrapper, "dev");

    MirEvent mir_event;
    uint32_t event_time = 14;
    float x1 = 15, x2 = 23;
    float y1 = 17, y2 = 21;

    EXPECT_CALL(mock_libinput, libinput_event_get_type(fake_event))
        .WillRepeatedly(Return(LIBINPUT_EVENT_POINTER_MOTION));
    EXPECT_CALL(mock_libinput, libinput_event_get_pointer_event(fake_event))
        .WillRepeatedly(Return(fake_pointer_event));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_time(fake_pointer_event))
        .WillRepeatedly(Return(event_time));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_dx(fake_pointer_event))
        .WillOnce(Return(x1))
        .WillOnce(Return(x2));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_dy(fake_pointer_event))
        .WillOnce(Return(y1))
        .WillOnce(Return(y2));
    EXPECT_CALL(mock_sink, handle_input(_))
        .WillRepeatedly(SaveArg<0>(&mir_event));

    dev.start(mock_registry, mock_sink);
    dev.process_event(fake_event);
    dev.process_event(fake_event);

    EXPECT_THAT(mir_event.motion.pointer_coordinates[0].x, Eq(x1+x2));
    EXPECT_THAT(mir_event.motion.pointer_coordinates[0].y, Eq(y1+y2));
}

TEST_F(LibInputDevice, process_event_handles_press_and_release)
{
    using namespace ::testing;
    mie::LibInputDevice dev(wrapper, "dev");

    MirEvent first, second, third, fourth;
    uint32_t event_time = 14;

    EXPECT_CALL(mock_libinput, libinput_event_get_type(fake_event))
        .WillRepeatedly(Return(LIBINPUT_EVENT_POINTER_BUTTON));
    EXPECT_CALL(mock_libinput, libinput_event_get_pointer_event(fake_event))
        .WillRepeatedly(Return(fake_pointer_event));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_time(fake_pointer_event))
        .WillRepeatedly(Return(event_time));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_button(fake_pointer_event))
        .WillOnce(Return(1))
        .WillOnce(Return(2))
        .WillOnce(Return(2))
        .WillOnce(Return(1));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_button_state(fake_pointer_event))
        .WillOnce(Return(LIBINPUT_BUTTON_STATE_PRESSED))
        .WillOnce(Return(LIBINPUT_BUTTON_STATE_PRESSED))
        .WillOnce(Return(LIBINPUT_BUTTON_STATE_RELEASED))
        .WillOnce(Return(LIBINPUT_BUTTON_STATE_RELEASED));
    EXPECT_CALL(mock_sink, handle_input(_))
        .WillOnce(SaveArg<0>(&first))
        .WillOnce(SaveArg<0>(&second))
        .WillOnce(SaveArg<0>(&third))
        .WillOnce(SaveArg<0>(&fourth));

    dev.start(mock_registry, mock_sink);
    dev.process_event(fake_event);
    dev.process_event(fake_event);
    dev.process_event(fake_event);
    dev.process_event(fake_event);

    EXPECT_THAT(first.motion.action, Eq(mir_motion_action_down));
    EXPECT_THAT(second.motion.action, Eq(mir_motion_action_down));
    EXPECT_THAT(third.motion.action, Eq(mir_motion_action_up));
    EXPECT_THAT(fourth.motion.action, Eq(mir_motion_action_up));
    EXPECT_THAT(first.motion.button_state,Eq(1));
    EXPECT_THAT(second.motion.button_state,Eq(1|2));
    EXPECT_THAT(third.motion.button_state,Eq(1));
    EXPECT_THAT(fourth.motion.button_state,Eq(0));
}

TEST_F(LibInputDevice, process_event_handles_scoll)
{
    using namespace ::testing;
    mie::LibInputDevice dev(wrapper, "dev");

    MirEvent first, second;
    uint32_t event_time = 14;

    EXPECT_CALL(mock_libinput, libinput_event_get_type(fake_event))
        .WillRepeatedly(Return(LIBINPUT_EVENT_POINTER_AXIS));
    EXPECT_CALL(mock_libinput, libinput_event_get_pointer_event(fake_event))
        .WillRepeatedly(Return(fake_pointer_event));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_time(fake_pointer_event))
        .WillRepeatedly(Return(event_time));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_axis(fake_pointer_event))
        .WillOnce(Return(LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
        .WillOnce(Return(LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL));
    EXPECT_CALL(mock_libinput, libinput_event_pointer_get_axis_value(fake_pointer_event))
        .WillOnce(Return(20.0f))
        .WillOnce(Return(5.0f));
    EXPECT_CALL(mock_sink, handle_input(_))
        .WillOnce(SaveArg<0>(&first))
        .WillOnce(SaveArg<0>(&second));

    dev.start(mock_registry, mock_sink);
    dev.process_event(fake_event);
    dev.process_event(fake_event);

    EXPECT_THAT(first.motion.action, Eq(mir_motion_action_scroll));
    EXPECT_THAT(second.motion.action, Eq(mir_motion_action_scroll));
    EXPECT_THAT(first.motion.pointer_coordinates[0].vscroll,Eq(20.0f));
    EXPECT_THAT(first.motion.pointer_coordinates[0].hscroll,Eq(0.0f));
    EXPECT_THAT(second.motion.pointer_coordinates[0].vscroll,Eq(0.0f));
    EXPECT_THAT(second.motion.pointer_coordinates[0].hscroll,Eq(5.0f));
}

