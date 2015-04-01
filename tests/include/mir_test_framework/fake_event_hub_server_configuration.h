/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 *              Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_TEST_FAKE_EVENT_HUB_SERVER_CONFIGURATION_H_
#define MIR_TEST_FAKE_EVENT_HUB_SERVER_CONFIGURATION_H_

#include "mir_test_framework/stubbed_server_configuration.h"

namespace mir
{
namespace input
{
namespace android
{
class FakeEventHub;
}
}
}

namespace mir_test_framework
{

class FakeEventHubServerConfiguration : public StubbedServerConfiguration
{
public:
    using StubbedServerConfiguration::StubbedServerConfiguration;

    std::shared_ptr<droidinput::EventHubInterface> the_event_hub() override;
    std::shared_ptr<mir::input::InputManager> the_input_manager() override;
    std::shared_ptr<mir::input::InputDispatcher> the_input_dispatcher() override;
    std::shared_ptr<mir::shell::InputTargeter> the_input_targeter() override;
    std::shared_ptr<mir::input::InputSender> the_input_sender() override;

    // TODO remove reliance on legacy window management
    auto the_window_manager_builder() -> shell::WindowManagerBuilder override;

    std::shared_ptr<mir::input::android::FakeEventHub> fake_event_hub;
};

}

#endif /* MIR_TEST_FAKE_EVENT_HUB_SERVER_CONFIGURATION_H_ */
