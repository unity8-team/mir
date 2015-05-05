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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/input/input_device_info.h"
#include "mir/input/event_filter.h"
#include "mir/input/composite_event_filter.h"

#include "mir_test_framework/fake_input_device.h"
#include "mir_test_framework/stub_server_platform_factory.h"
#include "mir_test_framework/headless_nested_server_runner.h"
#include "mir_test_framework/headless_in_process_server.h"
#include "mir_test_framework/using_stub_client_platform.h"
#include "mir_test_framework/any_surface.h"
#include "mir_test_doubles/nested_mock_egl.h"
#include "mir_test/event_factory.h"
#include "mir_test/fake_shared.h"
#include "mir_test/wait_condition.h"
#include "mir_test/spin_wait.h"

#include "mir_toolkit/mir_client_library.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <linux/input.h>

namespace mi = mir::input;
namespace mis = mi::synthesis;

namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace mtf = mir_test_framework;

using namespace ::testing;

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

std::vector<geom::Rectangle> const display_geometry
{
    {{  0, 0}, { 640,  480}},
    {{480, 0}, {1920, 1080}}
};
    
struct NestedServerWithMockEventFilter : mtf::HeadlessNestedServerRunner
{
    NestedServerWithMockEventFilter(std:: string const& connection_string)
        : mtf::HeadlessNestedServerRunner(connection_string)
    {
        start_server();
        server.the_composite_event_filter()->append(mock_event_filter);
        
    }
    ~NestedServerWithMockEventFilter()
    {
        stop_server();
    }

    std::shared_ptr<MockEventFilter> const mock_event_filter = std::make_shared<MockEventFilter>();
};

struct NestedInput : public mtf::HeadlessInProcessServer
{

    void SetUp()
    {
        initial_display_layout(display_geometry);
        mtf::HeadlessInProcessServer::SetUp();
    }
    
    mtd::NestedMockEGL mock_egl;
    mtf::UsingStubClientPlatform using_stub_client_platform;
    
    std::unique_ptr<mtf::FakeInputDevice> fake_keyboard{
        mtf::add_fake_input_device(mi::InputDeviceInfo{ 0, "keyboard", "keyboard-uid" , mi::DeviceCapability::keyboard})
    };

    mir::test::WaitCondition all_events_received;
};

struct ExposedSurface
{
public:
    ExposedSurface(std::string const& connect_string)
    {
        // Ensure the nested server posts a frame
        connection = mir_connect_sync(connect_string.c_str(), __PRETTY_FUNCTION__);
        surface = mtf::make_any_surface(connection);
        mir_buffer_stream_swap_buffers_sync(mir_surface_get_buffer_stream(surface));

        bool became_exposed_and_focused = mir::test::spin_wait_for_condition_or_timeout(
        [this]
        {
            return mir_surface_get_visibility(surface) == mir_surface_visibility_exposed
                && mir_surface_get_focus(surface) == mir_surface_focused;
        },
        std::chrono::seconds{10});
        EXPECT_TRUE(became_exposed_and_focused);
    }

    ~ExposedSurface()
    {
        mir_surface_release_sync(surface);
        mir_connection_release(connection);
    }
protected:
    ExposedSurface(ExposedSurface const&) = delete;
    ExposedSurface& operator=(ExposedSurface const&) = delete;
    
private:
    MirConnection *connection;
    MirSurface *surface;
};

}

TEST_F(NestedInput, nested_event_filter_receives_keyboard_from_host)
{
    NestedServerWithMockEventFilter nested_mir{new_connection()};
    ExposedSurface client(nested_mir.new_connection());

    InSequence seq;
    EXPECT_CALL(*nested_mir.mock_event_filter, handle(_)).Times(AtLeast(1)).WillOnce(Return(true));
    EXPECT_CALL(*nested_mir.mock_event_filter, handle(_)).Times(AtLeast(1)).WillOnce(
            DoAll(mt::WakeUp(&all_events_received), Return(true)));


    fake_keyboard->emit_event(mis::a_key_down_event().of_scancode(KEY_RIGHTSHIFT));
    fake_keyboard->emit_event(mis::a_key_up_event().of_scancode(KEY_RIGHTSHIFT));

    all_events_received.wait_for_at_most_seconds(10);
}
