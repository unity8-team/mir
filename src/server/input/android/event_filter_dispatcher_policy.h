/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */
#ifndef MIR_EVENT_FILTER_DISPATCHER_POLICY_H_
#define MIR_EVENT_FILTER_DISPATCHER_POLICY_H_

#include "mir/input/event_filter.h"

#include <InputDispatcher.h>

namespace android
{
class InputEvent;
}

namespace droidinput = android;

namespace mir
{
namespace input
{
namespace android
{
//class EventFilter;

class EventFilterDispatcherPolicy : public droidinput::InputDispatcherPolicyInterface
{
public:
    EventFilterDispatcherPolicy(std::shared_ptr<EventFilter> const& event_filter);
    virtual ~EventFilterDispatcherPolicy() {}

    void notifyConfigurationChanged(nsecs_t when);
    nsecs_t notifyANR(droidinput::sp<droidinput::InputApplicationHandle> const& inputApplicationHandle,
        droidinput::sp<droidinput::InputWindowHandle> const& inputWindowHandle);
    void notifyInputChannelBroken(droidinput::sp<droidinput::InputWindowHandle> const& inputWindowHandle);
    bool filterInputEvent(const droidinput::InputEvent* input_event,
        uint32_t policy_flags);
    void interceptKeyBeforeQueueing(const droidinput::KeyEvent* key_event,
        uint32_t& policy_flags);
    void getDispatcherConfiguration(droidinput::InputDispatcherConfiguration* outConfig);
    bool isKeyRepeatEnabled();
    void interceptMotionBeforeQueueing(nsecs_t when, uint32_t& policyFlags);

    nsecs_t interceptKeyBeforeDispatching(droidinput::sp<droidinput::InputWindowHandle> const& inputWindowHandle,
        droidinput::KeyEvent const* keyEvent, uint32_t policyFlags);

    bool dispatchUnhandledKey(droidinput::sp<droidinput::InputWindowHandle> const& inputWindowHandle,
                              droidinput::KeyEvent const* keyEvent, uint32_t policyFlags,
                              droidinput::KeyEvent* outFallbackKeyEvent);

    void notifySwitch(nsecs_t when, int32_t switchCode, int32_t switchValue, uint32_t policyFlags);
    void pokeUserActivity(nsecs_t eventTime, int32_t eventType);
    bool checkInjectEventsPermissionNonReentrant(int32_t injectorPid, int32_t injectorUid);

protected:
    EventFilterDispatcherPolicy(const EventFilterDispatcherPolicy&) = delete;
    EventFilterDispatcherPolicy& operator=(const EventFilterDispatcherPolicy&) = delete;
private:
    std::shared_ptr<EventFilter> event_filter;
};

}
}
}

#endif // MIR_DUMMY_INPUT_DISPATCHER_POLICY_H_
