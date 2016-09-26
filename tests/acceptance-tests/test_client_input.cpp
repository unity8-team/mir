/*
 * Copyright © 2015 Canonical Ltd.
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

#include "mir/input/input_device_info.h"
#include "mir/input/event_filter.h"
#include "mir/input/composite_event_filter.h"

#include "mir_test_framework/headless_in_process_server.h"
#include "mir_test_framework/fake_input_device.h"
#include "mir_test_framework/placement_applying_shell.h"
#include "mir_test_framework/stub_server_platform_factory.h"
#include "mir_test_framework/temporary_environment_value.h"
#include "mir/test/signal.h"
#include "mir/test/spin_wait.h"
#include "mir/test/event_matchers.h"
#include "mir/test/event_factory.h"

#include "mir_toolkit/mir_client_library.h"
#include "mir/events/event_builders.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <linux/input.h>

#include <condition_variable>
#include <chrono>
#include <mutex>

namespace mi = mir::input;
namespace mt = mir::test;
namespace ms = mir::scene;
namespace mis = mir::input::synthesis;
namespace mtf = mir_test_framework;
namespace geom = mir::geometry;

using namespace std::chrono_literals;

namespace
{

struct MockEventFilter : public mi::EventFilter
{
    // Work around GMock wanting to know how to construct MirEvent
    MOCK_METHOD1(handle, bool(MirEvent const*));
    bool handle(MirEvent const& ev)
    {
        handle(&ev);
        return true;
    }
};

const int surface_width = 100;
const int surface_height = 100;

void null_event_handler(MirSurface*, MirEvent const*, void*)
{
}

struct Client
{
    MirSurface* surface{nullptr};

    MOCK_METHOD1(handle_input, void(MirEvent const*));
    MOCK_METHOD1(handle_keymap, void(MirEvent const*));

    Client(std::string const& con, std::string const& name)
    {
        connection = mir_connect_sync(con.c_str(), name.c_str());

        if (!mir_connection_is_valid(connection))
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error{std::string{"Failed to connect to test server: "} +
                mir_connection_get_error_message(connection)});
        }
        auto spec = mir_connection_create_spec_for_normal_surface(connection, surface_width,
                                                                  surface_height, mir_pixel_format_abgr_8888);
        mir_surface_spec_set_name(spec, name.c_str());
        surface = mir_surface_create_sync(spec);
        mir_surface_spec_release(spec);
        if (!mir_surface_is_valid(surface))
            BOOST_THROW_EXCEPTION(std::runtime_error{std::string{"Failed creating a surface: "}+
                mir_surface_get_error_message(surface)});

        mir_surface_set_event_handler(surface, handle_event, this);
        mir_buffer_stream_swap_buffers_sync(
            mir_surface_get_buffer_stream(surface));

        ready_to_accept_events.wait_for(4s);
        if (!ready_to_accept_events.raised())
            BOOST_THROW_EXCEPTION(std::runtime_error("Timeout waiting for surface to become focused and exposed"));
    }

    void handle_surface_event(MirSurfaceEvent const* event)
    {
        auto const attrib = mir_surface_event_get_attribute(event);
        auto const value = mir_surface_event_get_attribute_value(event);

        if (mir_surface_attrib_visibility == attrib &&
            mir_surface_visibility_exposed == value)
            exposed = true;

        if (mir_surface_attrib_focus == attrib &&
            mir_surface_focused == value)
            focused = true;

        if (exposed && focused)
            ready_to_accept_events.raise();
    }

    static void handle_event(MirSurface*, MirEvent const* ev, void* context)
    {
        auto const client = static_cast<Client*>(context);
        auto type = mir_event_get_type(ev);
        if (type == mir_event_type_surface)
        {
            auto surface_event = mir_event_get_surface_event(ev);
            client->handle_surface_event(surface_event);

        }
        if (type == mir_event_type_input)
            client->handle_input(ev);
        if (type == mir_event_type_keymap)
            client->handle_keymap(ev);
    }
    ~Client()
    {
        // Remove the event handler to avoid handling spurious events unrelated
        // to the tests (e.g. pointer leave events when the surface is destroyed),
        // which can cause test expectations to fail.
        mir_surface_set_event_handler(surface, null_event_handler, nullptr);
        mir_surface_release_sync(surface);
        mir_connection_release(connection);
    }
    MirConnection * connection;
    mir::test::Signal ready_to_accept_events;
    mir::test::Signal all_events_received;
    bool exposed = false;
    bool focused = false;
};

struct TestClientInput : mtf::HeadlessInProcessServer
{
    void SetUp() override
    {
        initial_display_layout({screen_geometry});

        server.wrap_shell(
            [this](std::shared_ptr<mir::shell::Shell> const& wrapped)
            {
                shell = std::make_shared<mtf::PlacementApplyingShell>(wrapped, input_regions, positions);
                return shell;
            });

        HeadlessInProcessServer::SetUp();

        positions[first] = geom::Rectangle{{0,0}, {surface_width, surface_height}};
    }

    std::shared_ptr<mtf::PlacementApplyingShell> shell;
    std::string const keyboard_name = "keyboard";
    std::string const keyboard_unique_id = "keyboard-uid";
    std::string const mouse_name = "mouse";
    std::string const mouse_unique_id = "mouse-uid";
    std::string const touchscreen_name = "touchscreen";
    std::string const touchscreen_unique_id = "touchscreen-uid";
    std::unique_ptr<mtf::FakeInputDevice> fake_keyboard{mtf::add_fake_input_device(mi::InputDeviceInfo{
        keyboard_name, keyboard_unique_id, mi::DeviceCapability::keyboard | mi::DeviceCapability::alpha_numeric})};
    std::unique_ptr<mtf::FakeInputDevice> fake_mouse{
        mtf::add_fake_input_device(mi::InputDeviceInfo{mouse_name, mouse_unique_id, mi::DeviceCapability::pointer})};
    std::unique_ptr<mtf::FakeInputDevice> fake_touch_screen{mtf::add_fake_input_device(
        mi::InputDeviceInfo{touchscreen_name, touchscreen_unique_id,
                            mi::DeviceCapability::touchscreen | mi::DeviceCapability::multitouch})};

    std::string first{"first"};
    std::string second{"second"};
    mtf::ClientInputRegions input_regions;
    mtf::ClientPositions positions;
    geom::Rectangle screen_geometry{{0,0}, {1000,800}};
    std::shared_ptr<MockEventFilter> mock_event_filter = std::make_shared<MockEventFilter>();
};

}

using namespace ::testing;
using namespace std::chrono_literals;

TEST_F(TestClientInput, clients_receive_keys)
{
    Client first_client(new_connection(), first);

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_Shift_R))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_M))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyUpEvent(), mt::KeyOfSymbol(XKB_KEY_M))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyUpEvent(), mt::KeyOfSymbol(XKB_KEY_Shift_R))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_i))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyUpEvent(), mt::KeyOfSymbol(XKB_KEY_i))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_r))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyUpEvent(), mt::KeyOfSymbol(XKB_KEY_r)))).WillOnce(
        mt::WakeUp(&first_client.all_events_received));

    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_RIGHTSHIFT));
    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_M));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_M));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_RIGHTSHIFT));
    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_I));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_I));
    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_R));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_R));

    first_client.all_events_received.wait_for(10s);
}

TEST_F(TestClientInput, clients_receive_us_english_mapped_keys)
{
    Client first_client(new_connection(), first);

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_Shift_L))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_dollar))))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_LEFTSHIFT));
    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_4));
    first_client.all_events_received.wait_for(10s);
}

TEST_F(TestClientInput, clients_receive_pointer_inside_window_and_crossing_events)
{
    positions[first] = geom::Rectangle{{0,0}, {surface_width, surface_height}};
    Client first_client(new_connection(), first);

    // We should see the cursor enter
    InSequence seq;
    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent()));
    EXPECT_CALL(first_client, handle_input(mt::PointerEventWithPosition(surface_width - 1, surface_height - 1)));
    EXPECT_CALL(first_client, handle_input(mt::PointerLeaveEvent()))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));
    // But we should not receive an event for the second movement outside of our surface!

    fake_mouse->emit_event(mis::a_pointer_event().with_movement(surface_width - 1, surface_height - 1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(2, 2));

    first_client.all_events_received.wait_for(120s);
}

TEST_F(TestClientInput, clients_receive_relative_pointer_events)
{
    using namespace ::testing;

    mtf::TemporaryEnvironmentValue disable_batching("MIR_CLIENT_INPUT_RATE", "0");
    
    positions[first] = geom::Rectangle{{0,0}, {surface_width, surface_height}};
    Client first_client(new_connection(), first);

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent()));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(1, 1), mt::PointerEventWithDiff(1, 1))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(2, 2), mt::PointerEventWithDiff(1, 1))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(3, 3), mt::PointerEventWithDiff(1, 1))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(2, 2), mt::PointerEventWithDiff(-1, -1))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(1, 1), mt::PointerEventWithDiff(-1, -1))));
    // Ensure we continue to receive relative moement even when absolute movement is constrained.
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(0, 0), mt::PointerEventWithDiff(-1, -1))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(0, 0), mt::PointerEventWithDiff(-1, -1))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::PointerEventWithPosition(0, 0), mt::PointerEventWithDiff(-1, -1))))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1, 1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1, 1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1, 1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(-1, -1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(-1, -1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(-1, -1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(-1, -1));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(-1, -1));

    first_client.all_events_received.wait_for(120s);
}

TEST_F(TestClientInput, clients_receive_button_events_inside_window)
{
    Client first_client(new_connection(), first);

    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent()));
    // The cursor starts at (0, 0).
    EXPECT_CALL(first_client, handle_input(mt::ButtonDownEvent(0, 0)))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));

    first_client.all_events_received.wait_for(10s);
}

TEST_F(TestClientInput, clients_receive_many_button_events_inside_window)
{
    Client first_client(new_connection(), first);
    // The cursor starts at (0, 0).

    InSequence seq;
    auto expect_buttons = [&](MirPointerButtons b) {
        EXPECT_CALL(first_client, handle_input(mt::ButtonsDown(0, 0, b)));
    };

    MirPointerButtons buttons = mir_pointer_button_primary;
    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent()));
    expect_buttons(buttons);
    expect_buttons(buttons |= mir_pointer_button_secondary);
    expect_buttons(buttons |= mir_pointer_button_tertiary);
    expect_buttons(buttons |= mir_pointer_button_forward);
    expect_buttons(buttons |= mir_pointer_button_back);
    expect_buttons(buttons &= ~mir_pointer_button_back);
    expect_buttons(buttons &= ~mir_pointer_button_forward);
    expect_buttons(buttons &= ~mir_pointer_button_tertiary);
    expect_buttons(buttons &= ~mir_pointer_button_secondary);
    EXPECT_CALL(first_client, handle_input(mt::ButtonsDown(0, 0, 0))).WillOnce(
    mt::WakeUp(&first_client.all_events_received));

    auto press_button = [&](int button) {
        fake_mouse->emit_event(mis::a_button_down_event().of_button(button).with_action(mis::EventAction::Down));
    };
    auto release_button = [&](int button) {
        fake_mouse->emit_event(mis::a_button_up_event().of_button(button).with_action(mis::EventAction::Up));
    };
    press_button(BTN_LEFT);
    press_button(BTN_RIGHT);
    press_button(BTN_MIDDLE);
    press_button(BTN_FORWARD);
    press_button(BTN_BACK);
    release_button(BTN_BACK);
    release_button(BTN_FORWARD);
    release_button(BTN_MIDDLE);
    release_button(BTN_RIGHT);
    release_button(BTN_LEFT);

    first_client.all_events_received.wait_for(10s);
}

TEST_F(TestClientInput, multiple_clients_receive_pointer_inside_windows)
{
    int const screen_width = screen_geometry.size.width.as_int();
    int const screen_height = screen_geometry.size.height.as_int();
    int const client_height = screen_height / 2;
    int const client_width = screen_width / 2;

    positions[first] = {{0, 0}, {client_width, client_height}};
    positions[second] = {{client_width, client_height}, {client_width, client_height}};

    Client first_client(new_connection(), first);
    Client second_client(new_connection(), second);

    {
        InSequence seq;
        EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent()));
        EXPECT_CALL(first_client, handle_input(mt::PointerEventWithPosition(client_width - 1, client_height - 1)));
        EXPECT_CALL(first_client, handle_input(mt::PointerLeaveEvent()))
            .WillOnce(mt::WakeUp(&first_client.all_events_received));
    }

    {
        InSequence seq;
        EXPECT_CALL(second_client, handle_input(mt::PointerEnterEvent()));
        EXPECT_CALL(second_client, handle_input(mt::PointerEventWithPosition(client_width - 1, client_height - 1)))
            .WillOnce(mt::WakeUp(&second_client.all_events_received));
    }

    // In the bounds of the first surface
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(client_width - 1, client_height - 1));
    // In the bounds of the second surface
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(client_width, client_height));

    first_client.all_events_received.wait_for(2s);
    second_client.all_events_received.wait_for(2s);
}

TEST_F(TestClientInput, clients_do_not_receive_pointer_outside_input_region)
{
    int const client_height = surface_height;
    int const client_width = surface_width;

    input_regions[first] = {{{0, 0}, {client_width - 80, client_height}},
                            {{client_width - 20, 0}, {client_width - 80, client_height}}};

    Client first_client(new_connection(), first);

    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent())).Times(AnyNumber());
    EXPECT_CALL(first_client, handle_input(mt::PointerLeaveEvent())).Times(AnyNumber());
    EXPECT_CALL(first_client, handle_input(mt::PointerMovementEvent())).Times(AnyNumber());

    {
        // We should see two of the three button pairs.
        InSequence seq;
        EXPECT_CALL(first_client, handle_input(mt::ButtonDownEvent(1, 1)));
        EXPECT_CALL(first_client, handle_input(mt::ButtonUpEvent(1, 1)));
        EXPECT_CALL(first_client, handle_input(mt::ButtonDownEvent(99, 99)));
        EXPECT_CALL(first_client, handle_input(mt::ButtonUpEvent(99, 99)))
            .WillOnce(mt::WakeUp(&first_client.all_events_received));
    }

    // First we will move the cursor in to the input region on the left side of
    // the window. We should see a click here.
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1, 1));
    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
    fake_mouse->emit_event(mis::a_button_up_event().of_button(BTN_LEFT));

    // Now in to the dead zone in the center of the window. We should not see
    // a click here.
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(49, 49));
    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
    fake_mouse->emit_event(mis::a_button_up_event().of_button(BTN_LEFT));

    // Now in to the right edge of the window, in the right input region.
    // Again we should see a click.
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(49, 49));
    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
    fake_mouse->emit_event(mis::a_button_up_event().of_button(BTN_LEFT));

    first_client.all_events_received.wait_for(5s);
}

TEST_F(TestClientInput, scene_obscure_motion_events_by_stacking)
{
    auto smaller_geometry = screen_geometry;
    smaller_geometry.size.width =
        geom::Width{screen_geometry.size.width.as_uint32_t() / 2};

    positions[first] = screen_geometry;
    positions[second] = smaller_geometry;

    Client first_client(new_connection(), first);
    Client second_client(new_connection(), second);

    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent())).Times(AnyNumber());
    EXPECT_CALL(first_client, handle_input(mt::PointerLeaveEvent())).Times(AnyNumber());
    EXPECT_CALL(first_client, handle_input(mt::PointerMovementEvent())).Times(AnyNumber());
    {
        // We should only see one button event sequence.
        InSequence seq;
        EXPECT_CALL(first_client, handle_input(mt::ButtonDownEvent(501, 1)));
        EXPECT_CALL(first_client, handle_input(mt::ButtonUpEvent(501, 1)))
            .WillOnce(mt::WakeUp(&first_client.all_events_received));
    }

    EXPECT_CALL(second_client, handle_input(mt::PointerEnterEvent())).Times(AnyNumber());
    EXPECT_CALL(second_client, handle_input(mt::PointerLeaveEvent())).Times(AnyNumber());
    EXPECT_CALL(second_client, handle_input(mt::PointerMovementEvent())).Times(AnyNumber());
    {
        // Likewise we should only see one button sequence.
        InSequence seq;
        EXPECT_CALL(second_client, handle_input(mt::ButtonDownEvent(1, 1)));
        EXPECT_CALL(second_client, handle_input(mt::ButtonUpEvent(1, 1)))
            .WillOnce(mt::WakeUp(&second_client.all_events_received));
    }

    // First we will move the cursor in to the region where client 2 obscures client 1
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1, 1));
    fake_mouse->emit_event(
        mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
    fake_mouse->emit_event(mis::a_button_up_event().of_button(BTN_LEFT));
    // Now we move to the unobscured region of client 1
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(500, 0));
    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
    fake_mouse->emit_event(mis::a_button_up_event().of_button(BTN_LEFT));

    first_client.all_events_received.wait_for(5s);
    second_client.all_events_received.wait_for(5s);
}

TEST_F(TestClientInput, hidden_clients_do_not_receive_pointer_events)
{
    positions[second] = {{0,0}, {surface_width, surface_height}};

    Client first_client(new_connection(), first);
    Client second_client(new_connection(), second);

    EXPECT_CALL(second_client, handle_input(mt::PointerEnterEvent())).Times(AnyNumber());
    EXPECT_CALL(second_client, handle_input(mt::PointerLeaveEvent())).Times(AnyNumber());
    EXPECT_CALL(second_client, handle_input(mt::PointerEventWithPosition(1, 1)))
        .WillOnce(mt::WakeUp(&second_client.all_events_received));

    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent())).Times(AnyNumber());
    EXPECT_CALL(first_client, handle_input(mt::PointerLeaveEvent())).Times(AnyNumber());
    EXPECT_CALL(first_client, handle_input(mt::PointerEventWithPosition(2, 2)))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    // We send one event and then hide the surface on top before sending the next.
    // So we expect each of the two surfaces to receive one event
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1,1));

    second_client.all_events_received.wait_for(2s);

    server.the_shell()->focused_session()->hide();

    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1,1));
    first_client.all_events_received.wait_for(2s);
}

TEST_F(TestClientInput, clients_receive_pointer_within_coordinate_system_of_window)
{
    int const screen_width = screen_geometry.size.width.as_int();
    int const screen_height = screen_geometry.size.height.as_int();
    int const client_height = screen_height / 2;
    int const client_width = screen_width / 2;

    positions[first] = {{screen_width / 2, screen_height / 2}, {client_width, client_height}};

    Client first_client(new_connection(), first);

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(mt::PointerEnterEvent()));
    EXPECT_CALL(first_client, handle_input(mt::PointerEventWithPosition(80, 170)))
        .Times(AnyNumber())
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    server.the_shell()->focused_surface()->move_to(geom::Point{screen_width / 2 - 40, screen_height / 2 - 80});

    fake_mouse->emit_event(mis::a_pointer_event().with_movement(screen_width / 2 + 40, screen_height / 2 + 90));

    first_client.all_events_received.wait_for(2s);
}

// TODO: Consider tests for more input devices with custom mapping (i.e. joysticks...)
TEST_F(TestClientInput, usb_direct_input_devices_work)
{
    float const minimum_touch = mtf::FakeInputDevice::minimum_touch_axis_value;
    float const maximum_touch = mtf::FakeInputDevice::maximum_touch_axis_value;
    auto const display_width = screen_geometry.size.width.as_int();
    auto const display_height = screen_geometry.size.height.as_int();

    // We place a click 10% in to the touchscreens space in both axis,
    // and a second at 0,0. Thus we expect to see a click at
    // .1*screen_width/height and a second at zero zero.
    float const abs_touch_x_1 = minimum_touch + (maximum_touch - minimum_touch) * 0.1f;
    float const abs_touch_y_1 = minimum_touch + (maximum_touch - minimum_touch) * 0.1f;
    float const abs_touch_x_2 = 0;
    float const abs_touch_y_2 = 0;

    float const expected_scale_x = display_width / (maximum_touch - minimum_touch + 1.0f);
    float const expected_scale_y = display_height / (maximum_touch - minimum_touch + 1.0f);

    float const expected_motion_x_1 = expected_scale_x * abs_touch_x_1;
    float const expected_motion_y_1 = expected_scale_y * abs_touch_y_1;
    float const expected_motion_x_2 = expected_scale_x * abs_touch_x_2;
    float const expected_motion_y_2 = expected_scale_y * abs_touch_y_2;

    positions[first] = screen_geometry;

    Client first_client(new_connection(), first);

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(
        mt::TouchEvent(expected_motion_x_1, expected_motion_y_1)));
    EXPECT_CALL(first_client, handle_input(
        mt::TouchEventInDirection(expected_motion_x_1,
                                  expected_motion_y_1,
                                  expected_motion_x_2,
                                  expected_motion_y_2)))
        .Times(AnyNumber())
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    fake_touch_screen->emit_event(mis::a_touch_event()
                                  .at_position({abs_touch_x_1, abs_touch_y_1}));
    // Sleep here to trigger more failures (simulate slow machine)
    // TODO why would that cause failures?b
    std::this_thread::sleep_for(10ms);
    fake_touch_screen->emit_event(mis::a_touch_event()
                                  .with_action(mis::TouchParameters::Action::Move)
                                  .at_position({abs_touch_x_2, abs_touch_y_2}));

    first_client.all_events_received.wait_for(2s);
}

TEST_F(TestClientInput, receives_one_touch_event_per_frame)
{
    positions[first] = screen_geometry;
    Client first_client(new_connection(), first);

    int const frame_rate = 60;
    int const input_rate = 500;
    int const nframes = 100;
    int const nframes_error = 50;
    int const inputs_per_frame = input_rate / frame_rate;
    int const ninputs = nframes * inputs_per_frame;
    auto const frame_time = 1000ms / frame_rate;

    int received_input_events = 0;

    EXPECT_CALL(first_client, handle_input(_))
        .Times(Between(nframes-nframes_error, nframes+nframes_error))
        .WillRepeatedly(InvokeWithoutArgs(
            [&]()
            {
                ++received_input_events;
                if (received_input_events >= nframes-nframes_error)
                    first_client.all_events_received.raise();
            }));

    fake_touch_screen->emit_event(mis::a_touch_event()
                                  .at_position({0,0}));

    ASSERT_THAT(input_rate, Ge(2 * frame_rate));
    ASSERT_THAT(ninputs, Gt(2 * nframes));

    auto start_time = std::chrono::steady_clock::now();

    for (int i = 0; i < ninputs; ++i)
    {
        /*
         * Sleep until the correct time for the frame. We use sleep_until
         * so that even on a very slow system it will catch up to the
         * correct real time for the frame and not drift out causing
         * test failures.
         */
        int frame_no = i / inputs_per_frame;
        std::this_thread::sleep_until(start_time + frame_no*frame_time);

        int const x = i;
        int const y = 2 * i;
        fake_touch_screen->emit_event(mis::a_touch_event()
                                      .with_action(mis::TouchParameters::Action::Move)
                                      .at_position({x,y}));
    }

    // Wait for the expected minimum number of events (should be quick)
    ASSERT_TRUE(first_client.all_events_received.wait_for(20s));

    // The main thing we're testing for is that too many events don't arrive
    // so we wait a little to check the cooked event stream has stopped:
    std::this_thread::sleep_for(100 * frame_time);

    // Remove reference to local received_input_events
    Mock::VerifyAndClearExpectations(&first_client);

    float const client_input_events_per_frame =
        (float)received_input_events / nframes;
    EXPECT_THAT(client_input_events_per_frame, Gt(0.0f));
    EXPECT_THAT(client_input_events_per_frame, Lt(1.5f));
}

TEST_F(TestClientInput, send_mir_input_events_through_surface)
{
    Client first_client(new_connection(), first);

    EXPECT_CALL(first_client, handle_input(mt::KeyDownEvent()))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    auto key_event = mir::events::make_event(MirInputDeviceId{0}, 0ns, std::vector<uint8_t>{}, mir_keyboard_action_down, 0, KEY_M,
                                             mir_input_event_modifier_none);

    server.the_shell()->focused_surface()->consume(key_event.get());

    first_client.all_events_received.wait_for(2s);
}

TEST_F(TestClientInput, clients_receive_keymap_change_events)
{
    Client first_client(new_connection(), first);

    std::string const model = "pc105";
    std::string const layout = "dvorak";
    MirInputDeviceId const id = 1;

    EXPECT_CALL(first_client, handle_keymap(mt::KeymapEventForDevice(id)))
        .Times(1)
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    server.the_shell()->focused_surface()->set_keymap(id, model, layout, "", "");
    first_client.all_events_received.wait_for(2s);
}

TEST_F(TestClientInput, keymap_changes_change_keycode_received)
{
    Client first_client(new_connection(), first);

    MirInputDeviceId const id = 1;
    std::string const model = "pc105";
    std::string const layout = "us";
    std::string const variant = "dvorak";

    mt::Signal first_event_received,
        client_sees_keymap_change;

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_n))));
    EXPECT_CALL(first_client, handle_input(mt::KeyUpEvent()))
         .WillOnce(mt::WakeUp(&first_event_received));
    EXPECT_CALL(first_client, handle_keymap(mt::KeymapEventForDevice(id)))
        .WillOnce(mt::WakeUp(&client_sees_keymap_change));

    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_b))));
    EXPECT_CALL(first_client, handle_input(mt::KeyUpEvent()))
        .WillOnce(mt::WakeUp(&first_client.all_events_received));

    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_N));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_N));

    first_event_received.wait_for(60s);

    server.the_shell()->focused_surface()->set_keymap(id, model, layout, variant, "");

    client_sees_keymap_change.wait_for(60s);

    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_N));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_N));

    first_client.all_events_received.wait_for(5s);
}


TEST_F(TestClientInput, sends_no_wrong_keymaps_to_clients)
{
    Client first_client(new_connection(), first);

    MirInputDeviceId const id = 1;
    std::string const model = "thargoid207";
    std::string const layout = "polaris";

    mt::Signal first_event_received,
        client_sees_keymap_change;

    EXPECT_CALL(first_client, handle_keymap(mt::KeymapEventForDevice(id))).Times(0);

    EXPECT_THROW(
        {server.the_shell()->focused_surface()->set_keymap(id, model, layout, "", "");},
        std::runtime_error);
}

TEST_F(TestClientInput, event_filter_may_consume_events)
{
    std::shared_ptr<MockEventFilter> mock_event_filter = std::make_shared<MockEventFilter>();
    server.the_composite_event_filter()->append(mock_event_filter);

    Client first_client(new_connection(), first);

    EXPECT_CALL(*mock_event_filter, handle(mt::InputConfigurationEvent())).Times(AnyNumber()).WillRepeatedly(Return(false));

    InSequence seq;
    EXPECT_CALL(*mock_event_filter, handle(_)).WillOnce(Return(true));
    EXPECT_CALL(*mock_event_filter, handle(_)).WillOnce(
        DoAll(mt::WakeUp(&first_client.all_events_received), Return(true)));

    // Since we handle the events in the filter the client should not receive them.
    EXPECT_CALL(first_client, handle_input(_)).Times(0);

    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_M));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_M));

    first_client.all_events_received.wait_for(10s);
}

namespace
{
struct TestClientInputKeyRepeat : public TestClientInput
{
    TestClientInputKeyRepeat()
        : enable_key_repeat("MIR_SERVER_ENABLE_KEY_REPEAT", "true")
    {
    }
    mtf::TemporaryEnvironmentValue enable_key_repeat;
};
}

TEST_F(TestClientInputKeyRepeat, keys_are_repeated_to_clients)
{
    using namespace testing;

    Client first_client(new_connection(), first);

    InSequence seq;
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyDownEvent(), mt::KeyOfSymbol(XKB_KEY_Shift_R))));
    EXPECT_CALL(first_client, handle_input(AllOf(mt::KeyRepeatEvent(),
        mt::KeyOfSymbol(XKB_KEY_Shift_R)))).WillOnce(mt::WakeUp(&first_client.all_events_received));
    // Extra repeats before we shut down.
    EXPECT_CALL(first_client, handle_input(mt::KeyRepeatEvent())).Times(AnyNumber());

    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_RIGHTSHIFT));

    first_client.all_events_received.wait_for(10s);
}

TEST_F(TestClientInput, pointer_events_pass_through_shaped_out_regions_of_client)
{
    using namespace testing;

    positions[first] = {{0, 0}, {10, 10}};
    
    Client client(new_connection(), first);

    MirRectangle input_rects[] = {{1, 1, 10, 10}};

    auto spec = mir_connection_create_spec_for_changes(client.connection);
    mir_surface_spec_set_input_shape(spec, input_rects, 1);
    mir_surface_apply_spec(client.surface, spec);
    mir_surface_spec_release(spec);

    ASSERT_TRUE(shell->wait_for_modify_surface(5s));

    // We verify that we don't receive the first shaped out button event.
    EXPECT_CALL(client, handle_input(mt::PointerEnterEvent()));
    EXPECT_CALL(client, handle_input(mt::PointerEventWithPosition(1, 1)));
    EXPECT_CALL(client, handle_input(mt::ButtonDownEvent(1, 1)))
        .WillOnce(mt::WakeUp(&client.all_events_received));
    

    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT).with_action(mis::EventAction::Down));
    fake_mouse->emit_event(mis::a_button_up_event().of_button(BTN_LEFT).with_action(mis::EventAction::Up));
    fake_mouse->emit_event(mis::a_pointer_event().with_movement(1, 1));
    fake_mouse->emit_event(mis::a_button_down_event().of_button(BTN_LEFT));

    client.all_events_received.wait_for(10s);
}

MATCHER_P3(ADeviceMatches, name, unique_id, caps, "")
{
    auto one_of_the_devices_matched = false;
    for (size_t i = 0, e = mir_input_config_device_count(arg); i != e; ++i)
    {
        auto dev = mir_input_config_get_device(arg, i);
        if (mir_input_device_get_name(dev) == name &&
            mir_input_device_get_unique_id(dev) == unique_id &&
            mir_input_device_get_capabilities(dev) == caps)
            one_of_the_devices_matched = true;
    }
    return one_of_the_devices_matched;
}

//Poll for the expected config to fix lp: #1555708. Client can't expect synchronization
//with the server on the input config.
TEST_F(TestClientInput, client_input_config_request_receives_all_attached_devices)
{
    auto con = mir_connect_sync(new_connection().c_str(), first.c_str());
    auto config = mir_connection_create_input_config(con);
    int limit = 10;
    int num_devices = 0;
    int expected_devices = 3;
 
    for(auto i = 0; i < limit; i++)
    {
        num_devices = mir_input_config_device_count(config);
        if (num_devices == expected_devices)
            break;
 
        std::this_thread::sleep_for(10ms);
        mir_input_config_destroy(config);
        config = mir_connection_create_input_config(con);
    }

    ASSERT_THAT(mir_input_config_device_count(config), Eq(expected_devices));

    EXPECT_THAT(config, ADeviceMatches(keyboard_name, keyboard_unique_id,
                                       uint32_t(mir_input_device_capability_keyboard |
                                                mir_input_device_capability_alpha_numeric)));
    EXPECT_THAT(config, ADeviceMatches(mouse_name, mouse_unique_id, mir_input_device_capability_pointer));
    EXPECT_THAT(config, ADeviceMatches(touchscreen_name, touchscreen_unique_id,
                                        uint32_t(mir_input_device_capability_touchscreen |
                                                 mir_input_device_capability_multitouch)));

    mir_input_config_destroy(config);
    mir_connection_release(con);
}


TEST_F(TestClientInput, callback_function_triggered_on_input_device_addition)
{
    Client a_client(new_connection(), first);
    mt::Signal callback_triggered;
    mir_connection_set_input_config_change_callback(
        a_client.connection,
        [](MirConnection*, void* cond)
        {
            static_cast<mt::Signal*>(cond)->raise();
        },
        static_cast<void*>(&callback_triggered));

    std::string const touchpad{"touchpad"};
    std::string const touchpad_uid{"touchpad"};
    std::unique_ptr<mtf::FakeInputDevice> fake_touchpad{mtf::add_fake_input_device(
        mi::InputDeviceInfo{touchpad, touchpad_uid,
                            mi::DeviceCapability::touchpad | mi::DeviceCapability::pointer})};

    callback_triggered.wait_for(1s);
    EXPECT_THAT(callback_triggered.raised(), Eq(true));

    auto config = mir_connection_create_input_config(a_client.connection);
    EXPECT_THAT(mir_input_config_device_count(config), Eq(4));
    EXPECT_THAT(config, ADeviceMatches(touchpad, touchpad_uid, uint32_t(mir_input_device_capability_touchpad |
                                                                         mir_input_device_capability_pointer)));

    mir_input_config_destroy(config);
}

TEST_F(TestClientInput, callback_function_triggered_on_input_device_removal)
{
    Client a_client(new_connection(), first);
    mt::Signal callback_triggered;
    mir_connection_set_input_config_change_callback(
        a_client.connection,
        [](MirConnection*, void* cond)
        {
            static_cast<mt::Signal*>(cond)->raise();
        },
        static_cast<void*>(&callback_triggered));

    fake_keyboard->emit_device_removal();
    callback_triggered.wait_for(1s);

    EXPECT_THAT(callback_triggered.raised(), Eq(true));

    auto config = mir_connection_create_input_config(a_client.connection);
    EXPECT_THAT(mir_input_config_device_count(config), Eq(2));
    mir_input_config_destroy(config);
}
