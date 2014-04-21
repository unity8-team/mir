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

#include "src/server/input/android/android_input_channel.h"
#include "src/server/input/android/input_sender.h"

#include "mir_test_doubles/mock_main_loop.h"
#include "mir_test_doubles/mock_input_surface.h"
#include "mir_test_doubles/mock_input_send_observer.h"
#include "mir_test/fake_shared.h"

#include "mir/input/input_send_entry.h"

#include "androidfw/Input.h"
#include "androidfw/InputTransport.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <list>
#include <tuple>
#include <cstring>

namespace mi = mir::input;
namespace mia = mi::android;
namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace droidinput = android;

namespace
{

class MockInputEventFactory : public droidinput::InputEventFactoryInterface
{
public:
    MOCK_METHOD0(createKeyEvent, droidinput::KeyEvent*());
    MOCK_METHOD0(createMotionEvent, droidinput::MotionEvent*());
};

class TriggeredMainLoop : public mir::graphics::EventHandlerRegister
{
public:
    typedef std::function<void(int)> callback;
    void register_signal_handler(std::initializer_list<int>, callback const&) override
    {
    }

    void register_fd_handler(std::initializer_list<int> fds, callback const& handler) override
    {
        for (int fd : fds)
        {
            fd_callbacks.push_back(std::make_pair(fd, handler));
        }
    }

    void trigger_pending_fds()
    {

        fd_set read_fds;
        FD_ZERO(&read_fds);
        int max_fd = 0;

        for(auto const & item : fd_callbacks)
        {
            FD_SET(item.first, &read_fds);
            max_fd = std::max(item.first, max_fd);
        }

        struct timeval do_not_wait{0, 0};

        if( select(max_fd+1, &read_fds, nullptr, nullptr, &do_not_wait) )
        {
            for(auto const & item : fd_callbacks)
            {
                FD_ISSET(item.first, &read_fds);
                item.second(item.first);
            }
        }
    }

    std::list<std::pair<int,callback>> fd_callbacks;
};

}


class TestAndroidInputSender : public ::testing::Test
{
public:
    TestAndroidInputSender() :
        clientChannel(new droidinput::InputChannel(droidinput::String8("test"), channel.client_fd())),
        consumer(clientChannel)
    {
        using namespace ::testing;

        std::memset(&key_event, 0, sizeof(MirEvent));
        std::memset(&motion_event, 0, sizeof(MirEvent));
        std::memset(&client_motion_event, 0, sizeof(droidinput::MotionEvent));
        std::memset(&client_key_event, 0, sizeof(droidinput::KeyEvent));

        key_event.type = mir_event_type_key;
        key_event.key.scan_code = 32;
        key_event.key.action = mir_key_action_down;

        motion_event.type = mir_event_type_motion;
        motion_event.motion.pointer_count = 2;
        motion_event.motion.device_id = 3;
        motion_event.motion.pointer_coordinates[0].x = 12;
        motion_event.motion.pointer_coordinates[0].y = 23;
        motion_event.motion.pointer_coordinates[0].pressure = 50;
        motion_event.motion.pointer_coordinates[0].tool_type = mir_motion_tool_type_finger;
        motion_event.motion.pointer_coordinates[1].tool_type = mir_motion_tool_type_finger;
        motion_event.motion.pointer_coordinates[1].x = 55;
        motion_event.motion.pointer_coordinates[1].y = 42;
        motion_event.motion.pointer_coordinates[1].pressure = 50;


        ON_CALL(event_factory,createKeyEvent()).WillByDefault(Return(&client_key_event));
        ON_CALL(event_factory,createMotionEvent()).WillByDefault(Return(&client_motion_event));
    }
    mia::AndroidInputChannel channel;
    droidinput::sp<droidinput::InputChannel> clientChannel;
    droidinput::InputConsumer consumer;

    testing::NiceMock<mtd::MockMainLoop> loop;
    testing::NiceMock<mtd::MockInputSurface> surface;
    testing::NiceMock<mtd::MockInputSendObserver> observer;

    MirEvent key_event;
    MirEvent motion_event;

    droidinput::MotionEvent client_motion_event;
    droidinput::KeyEvent client_key_event;

    testing::NiceMock<MockInputEventFactory> event_factory;

    droidinput::InputEvent * event= nullptr;
    uint32_t seq = 0;
};

TEST_F(TestAndroidInputSender,can_send_consumeable_mir_key_events)
{
    using namespace ::testing;
    mia::InputSender sender(mt::fake_shared(loop));

    std::shared_ptr<mi::InputSendEntry> entry = sender.send_event(key_event, surface, mt::fake_shared(channel));

    EXPECT_NE(nullptr,entry);

    EXPECT_EQ(droidinput::OK, consumer.consume(&event_factory, true, -1, &seq, &event));

    EXPECT_EQ(entry->sequence_id, seq);
    EXPECT_EQ(&client_key_event, event);
    EXPECT_EQ(key_event.key.scan_code, client_key_event.getScanCode());
}

TEST_F(TestAndroidInputSender,can_send_consumeable_mir_motion_events)
{
    using namespace ::testing;
    mia::InputSender sender(mt::fake_shared(loop));

    std::shared_ptr<mi::InputSendEntry> entry = sender.send_event(motion_event, surface, mt::fake_shared(channel));

    EXPECT_NE(nullptr,entry);
    EXPECT_EQ(droidinput::OK, consumer.consume(&event_factory, true, -1, &seq, &event));

    EXPECT_EQ(entry->sequence_id, seq);
    EXPECT_EQ(&client_motion_event, event);
    EXPECT_EQ(motion_event.motion.pointer_count, client_motion_event.getPointerCount());
    EXPECT_EQ(motion_event.motion.device_id, client_motion_event.getDeviceId());
    EXPECT_EQ(0, client_motion_event.getXOffset());
    EXPECT_EQ(0, client_motion_event.getYOffset());
    EXPECT_EQ(motion_event.motion.pointer_coordinates[0].x, client_motion_event.getRawX(0));
    EXPECT_EQ(motion_event.motion.pointer_coordinates[0].x, client_motion_event.getX(0));
    EXPECT_EQ(motion_event.motion.pointer_coordinates[0].y, client_motion_event.getRawY(0));
    EXPECT_EQ(motion_event.motion.pointer_coordinates[0].y, client_motion_event.getY(0));

    EXPECT_EQ(motion_event.motion.pointer_coordinates[1].x, client_motion_event.getRawX(1));
    EXPECT_EQ(motion_event.motion.pointer_coordinates[1].x, client_motion_event.getX(1));
    EXPECT_EQ(motion_event.motion.pointer_coordinates[1].y, client_motion_event.getRawY(1));
    EXPECT_EQ(motion_event.motion.pointer_coordinates[1].y, client_motion_event.getY(1));
}

TEST_F(TestAndroidInputSender, finish_signal_triggers_success_callback_as_consumed)
{
    TriggeredMainLoop fake_loop;
    mia::InputSender sender(mt::fake_shared(fake_loop));
    sender.set_send_observer(mt::fake_shared(observer));

    std::shared_ptr<mi::InputSendEntry> entry = sender.send_event(motion_event, surface, mt::fake_shared(channel));

    EXPECT_NE(nullptr,entry);

    EXPECT_EQ(droidinput::OK, consumer.consume(&event_factory, true, -1, &seq, &event));
    EXPECT_CALL(observer,send_suceeded(entry,mi::InputSendObserver::Consumed));

    consumer.sendFinishedSignal(seq, true);
    fake_loop.trigger_pending_fds();
}

TEST_F(TestAndroidInputSender, finish_signal_triggers_success_callback_as_not_consumed)
{
    TriggeredMainLoop fake_loop;
    mia::InputSender sender(mt::fake_shared(fake_loop));
    sender.set_send_observer(mt::fake_shared(observer));

    std::shared_ptr<mi::InputSendEntry> entry = sender.send_event(motion_event, surface, mt::fake_shared(channel));

    EXPECT_NE(nullptr,entry);

    EXPECT_EQ(droidinput::OK, consumer.consume(&event_factory, true, -1, &seq, &event));
    EXPECT_CALL(observer,send_suceeded(entry,mi::InputSendObserver::NotConsumed));

    consumer.sendFinishedSignal(seq, false);
    fake_loop.trigger_pending_fds();
}
