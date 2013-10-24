/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#include <InputDispatcher.h>
#include <mir/input/input_report.h>

namespace droidinput = android; // wonder why do we do this renaming throughout mir
#include <mir_test_doubles/stub_input_handles.h>

#include <mir/logging/logger.h>
#include <mir/logging/input_report.h>

#include <gtest/gtest.h>

#include <iostream>

using std::string;
using namespace android;
using namespace mir::test::doubles;

namespace
{
class TestLogger : public mir::logging::Logger
{
public:
    void log(Severity severity, const string& message, const string& component) override {
        std::cout << message << std::endl;
        (void)severity;
        (void)message;
        (void)component;
    }
};

class FakeTimeSource : public TimeSourceInterface
{
public:
    FakeTimeSource() : fakeNow(0) {}
    nsecs_t now() { return fakeNow; }
    nsecs_t fakeNow;
};

class InputDispatcherPolicy : public InputDispatcherPolicyInterface
{
public:
    InputDispatcherPolicy() {}
    virtual ~InputDispatcherPolicy() {}

    void notifyConfigurationChanged(nsecs_t when) override;
    nsecs_t notifyANR(const sp<InputApplicationHandle>& inputApplicationHandle,
            const sp<InputWindowHandle>& inputWindowHandle) override;
    void notifyInputChannelBroken(const sp<InputWindowHandle>& inputWindowHandle) override;
    void getDispatcherConfiguration(InputDispatcherConfiguration* outConfig) override;
    bool isKeyRepeatEnabled() override;
    bool filterInputEvent(const InputEvent* inputEvent, uint32_t policyFlags) override;
    void interceptKeyBeforeQueueing(const KeyEvent* keyEvent, uint32_t& policyFlags) override;
    void interceptMotionBeforeQueueing(nsecs_t when, uint32_t& policyFlags) override;
    nsecs_t interceptKeyBeforeDispatching(const sp<InputWindowHandle>& inputWindowHandle,
            const KeyEvent* keyEvent, uint32_t policyFlags) override;
    bool dispatchUnhandledKey(const sp<InputWindowHandle>& inputWindowHandle,
            const KeyEvent* keyEvent, uint32_t policyFlags, KeyEvent* outFallbackKeyEvent) override;
    void notifySwitch(nsecs_t when,
            int32_t switchCode, int32_t switchValue, uint32_t policyFlags) override;
    void pokeUserActivity(nsecs_t eventTime, int32_t eventType) override;
    bool checkInjectEventsPermissionNonReentrant(
            int32_t injectorPid, int32_t injectorUid) override;
};

class DummyInputReport : public mir::input::InputReport
{
public:
    DummyInputReport() {}

    void received_event_from_kernel(int64_t when, int type, int code, int value) override
    {
        (void)when;
        (void)type;
        (void)code;
        (void)value;
    }

    void published_key_event(int dest_fd, uint32_t seq_id, int64_t event_time) override
    {
        (void)dest_fd;
        (void)seq_id;
        (void)event_time;
    }

    void published_motion_event(int dest_fd, uint32_t seq_id, int64_t event_time) override
    {
        (void)dest_fd;
        (void)seq_id;
        (void)event_time;
    }

    void received_event_finished_signal(int src_fd, uint32_t seq_id) override
    {
        (void)src_fd;
        (void)seq_id;
    }
};

class FakeInputWindowEnumerator : public InputEnumerator
{
public:
    FakeInputWindowEnumerator() : InputEnumerator() {}
    void for_each(std::function<void(sp<InputWindowHandle> const&)> const& callback) override
    {
        for (auto it = stack.begin(); it != stack.end(); ++it)
        {
            callback(*it);
        }
    }

    std::vector< sp<InputWindowHandle> > stack;
};

struct AndroidInputDispatcher : public ::testing::Test
{
    void SetUp()
    {
        mir::logging::legacy_input_report::initialize(std::make_shared<TestLogger>());
        dummy_input_report = std::make_shared<DummyInputReport>();
        input_dispatcher_policy = new InputDispatcherPolicy;
    }

    std::shared_ptr<DummyInputReport> dummy_input_report;
    sp<InputDispatcherPolicy> input_dispatcher_policy;
};
} // namespace {

/******************************************************************************
 * InputDispatcherPolicy
 ******************************************************************************/

void InputDispatcherPolicy::notifyConfigurationChanged(nsecs_t when)
{
    (void)when;
}

nsecs_t InputDispatcherPolicy::notifyANR(
    const sp<InputApplicationHandle>& inputApplicationHandle,
    const sp<InputWindowHandle>& inputWindowHandle)
{
    (void)inputApplicationHandle;
    (void)inputWindowHandle;
    return 0;
}

void InputDispatcherPolicy::notifyInputChannelBroken(
    const sp<InputWindowHandle>& inputWindowHandle)
{
    (void)inputWindowHandle;
}

void InputDispatcherPolicy::getDispatcherConfiguration(InputDispatcherConfiguration* outConfig)
{
   (void)outConfig;
}

bool InputDispatcherPolicy::isKeyRepeatEnabled()
{
    return true;
}

bool InputDispatcherPolicy::filterInputEvent(const InputEvent* inputEvent, uint32_t policyFlags)
{
    (void)inputEvent;
    (void)policyFlags;
    return true;
}

void InputDispatcherPolicy::interceptKeyBeforeQueueing(const KeyEvent* keyEvent,
                                                       uint32_t& policyFlags)
{
    (void)keyEvent;
    policyFlags = POLICY_FLAG_PASS_TO_USER;
}

void InputDispatcherPolicy::interceptMotionBeforeQueueing(nsecs_t when, uint32_t& policyFlags)
{
    (void)when;
    policyFlags = POLICY_FLAG_PASS_TO_USER;
}

nsecs_t InputDispatcherPolicy::interceptKeyBeforeDispatching(
    const sp<InputWindowHandle>& inputWindowHandle,
    const KeyEvent* keyEvent, uint32_t policyFlags)
{
    (void)inputWindowHandle;
    (void)keyEvent;
    (void)policyFlags;
    return 0;
}

bool InputDispatcherPolicy::dispatchUnhandledKey(const sp<InputWindowHandle>& inputWindowHandle,
        const KeyEvent* keyEvent, uint32_t policyFlags, KeyEvent* outFallbackKeyEvent)
{
    (void)inputWindowHandle;
    (void)keyEvent;
    (void)policyFlags;
    (void)outFallbackKeyEvent;
    return false;
}

void InputDispatcherPolicy::notifySwitch(nsecs_t when,
        int32_t switchCode, int32_t switchValue, uint32_t policyFlags)
{
    (void)when;
    (void)switchCode;
    (void)switchValue;
    (void)policyFlags;
}

void InputDispatcherPolicy::pokeUserActivity(nsecs_t eventTime, int32_t eventType)
{
    (void)eventTime;
    (void)eventType;
}

bool InputDispatcherPolicy::checkInjectEventsPermissionNonReentrant(
        int32_t injectorPid, int32_t injectorUid)
{
    (void)injectorPid;
    (void)injectorUid;
    return true;
}

/******************************************************************************
 * The Actual Tests
 ******************************************************************************/

TEST_F(AndroidInputDispatcher, hooray)
{
    sp<InputDispatcher> input_dispatcher = new InputDispatcher(input_dispatcher_policy, dummy_input_report);

    sp<FakeTimeSource> time_source = new FakeTimeSource;
    input_dispatcher->setTimeSource(time_source);

    sp<StubWindowHandle> input_window = new StubWindowHandle;
    input_window->info()->touchableRegionLeft = 0;
    input_window->info()->touchableRegionRight = 100;
    input_window->info()->touchableRegionTop = 0;
    input_window->info()->touchableRegionBottom = 100;
    input_window->info()->visible = true;
    input_window->info()->paused = false;

    int server_fd, client_fd;
    status_t status = InputChannel::openInputFdPair(server_fd, client_fd);
    ASSERT_EQ(OK, status);

    sp<InputChannel> client_channel = new InputChannel("bar", client_fd);
    std::shared_ptr<InputConsumer> input_consumer = std::make_shared<InputConsumer>(client_channel);
    
    input_window->info()->inputChannel = new InputChannel("foo", server_fd);

    sp<FakeInputWindowEnumerator> input_window_enumerator = new FakeInputWindowEnumerator;
    input_dispatcher->setInputEnumerator(input_window_enumerator);

    input_window_enumerator->stack.push_back(input_window);

    input_dispatcher->registerInputChannel(input_window->getInfo()->inputChannel, input_window, false);

    NotifyMotionArgs motionArgs;
    motionArgs.eventTime = 0;
    motionArgs.deviceId = 0;
    motionArgs.source = AINPUT_SOURCE_TOUCHSCREEN;
    motionArgs.policyFlags = 0;
    motionArgs.action = AMOTION_EVENT_ACTION_DOWN;
    motionArgs.flags = 0;
    motionArgs.metaState = 0;
    motionArgs.buttonState = 0;
    motionArgs.edgeFlags = 0;
    motionArgs.pointerCount = 1;
    motionArgs.pointerProperties[0].id = 0;
    motionArgs.pointerProperties[0].toolType = AMOTION_EVENT_TOOL_TYPE_FINGER;
    motionArgs.pointerCoords[0].clear();
    motionArgs.pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_X, 10.0f);
    motionArgs.pointerCoords[0].setAxisValue(AMOTION_EVENT_AXIS_Y, 20.0f);
    motionArgs.xPrecision = 0;
    motionArgs.yPrecision = 0;
    motionArgs.downTime = 0;
    input_dispatcher->notifyMotion(&motionArgs);

    input_dispatcher->setInputDispatchMode(true /* enabled */, false /* frozen */);
    input_dispatcher->dispatchOnce();

    uint32_t sequence_number;
    InputEvent* event;
    PreallocatedInputEventFactory input_event_factory;
    status = input_consumer->consume(&input_event_factory, true /* consumeBatches */,
                                     -1 /* frameTime */, &sequence_number, &event);
    ASSERT_EQ(OK, status);

    status = input_consumer->sendFinishedSignal(sequence_number, true);
    ASSERT_EQ(OK, status);

    close(server_fd);
    close(client_fd);
}


