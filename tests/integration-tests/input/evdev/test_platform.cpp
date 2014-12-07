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

#include "src/platform/input/evdev/platform.h"
#include "src/server/report/null_report_factory.h"
#include "src/platform/input/evdev/input_device_factory.h"

#include "mir/input/input_device.h"
#include "mir/udev/wrapper.h"

#include "mir_test_doubles/mock_main_loop.h"
#include "mir_test_doubles/mock_input_device_registry.h"
#include "mir_test_doubles/mock_input_event_handler_register.h"
#include "mir_test_doubles/mock_libinput.h"
#include "mir_test_framework/udev_environment.h"

#include <thread>
#include <chrono>
#include <unistd.h>
#include <fcntl.h>

#include <gmock/gmock.h>

namespace mi = mir::input;
namespace mie = mi::evdev;
namespace mr = mir::report;
namespace mtd = mir::test::doubles;

namespace
{

struct EvdevPlatformBase
{
public:
    EvdevPlatformBase()
        : platform(mie::create_evdev_input_platform(mr::null_input_report()))
    {
    }
    mir_test_framework::UdevEnvironment env; // has to be created before platform
    mtd::MockLibInput mock_libinput;
    std::unique_ptr<mie::Platform> platform;
    ::testing::NiceMock<mtd::MockInputEventHandlerRegister> mock_event_handler_register;
    std::shared_ptr<::testing::NiceMock<mtd::MockInputDeviceRegistry>> mock_registry =
        std::make_shared<::testing::NiceMock<mtd::MockInputDeviceRegistry>>();
};

struct EvdevPlatform : ::testing::Test, EvdevPlatformBase
{
};

struct EvdevPlatformDeviceEvents : ::testing::TestWithParam<char const*>, EvdevPlatformBase
{
    EvdevPlatformDeviceEvents()
        : ::testing::TestWithParam<char const*>(), EvdevPlatformBase()
    {
        using namespace ::testing;
        ON_CALL(mock_event_handler_register, register_fd_handler_(_,_,_))
            .WillByDefault(Invoke(
                    [this](std::initializer_list<int> fd_list, void const*, std::function<void(int)> const& handler)
                    {
                        int fd = *fd_list.begin();
                        fd_callbacks.push_back([=]() { handler(fd); });
                    }));
        ON_CALL(mock_event_handler_register, register_handler_(_))
            .WillByDefault(Invoke(
                    [this](std::function<void()> const& action)
                    {
                        actions.push_back(action);
                    }));
    }

    void remove_device()
    {
        mir::udev::Enumerator devices{std::make_shared<mir::udev::Context>()};
        devices.scan_devices();

        for (auto& device : devices)
        {
            /*
             * Remove just the device providing dev/input/event*
             * If we remove more, it's possible that we'll remove the parent of the
             * /dev/input device, and umockdev will not generate a remove event
             * in that case.
             */
            if (device.devnode() && (std::string(device.devnode()).find("input/event") != std::string::npos))
            {
                env.remove_device((std::string("/sys") + device.devpath()).c_str());
            }
        }
    }

    void process_pending_actions()
    {
        decltype(actions) actions_to_execute;
        std::swap(actions, actions_to_execute);

        for(auto const& action : actions_to_execute)
            action();
    }

    void process_pending_fd_callbacks()
    {
        decltype(actions) actions_to_execute;

        actions_to_execute = fd_callbacks;

        for(auto const& action : actions_to_execute)
            action();
    }

    void process_pending()
    {
        process_pending_actions();
        process_pending_fd_callbacks();
    }

    std::vector<std::function<void()>> fd_callbacks;
    std::vector<std::function<void()>> actions;
};

}

TEST_F(EvdevPlatform, registers_to_event_handler_register_on_start)
{
    using namespace ::testing;
    EXPECT_CALL(mock_event_handler_register, register_fd_handler_(_,_,_));
    platform->start_monitor_devices(mock_event_handler_register, mock_registry);
}

TEST_F(EvdevPlatform, unregisters_to_event_handler_register_on_stop)
{
    using namespace ::testing;
    EXPECT_CALL(mock_event_handler_register, unregister_fd_handler(_));
    platform->stop_monitor_devices(mock_event_handler_register);
}

TEST_P(EvdevPlatformDeviceEvents, finds_device_on_start)
{
    using namespace ::testing;
    env.add_standard_device(GetParam());

    EXPECT_CALL(*mock_registry, add_device(_)).Times(1);
    platform->start_monitor_devices(mock_event_handler_register, mock_registry);

    process_pending();
}

TEST_P(EvdevPlatformDeviceEvents, adds_device_on_hotplug)
{
    using namespace ::testing;
    EXPECT_CALL(*mock_registry, add_device(_)).Times(1);
    platform->start_monitor_devices(mock_event_handler_register, mock_registry);
    process_pending();

    env.add_standard_device(GetParam());

    process_pending();
}

TEST_P(EvdevPlatformDeviceEvents, removes_device_on_hotplug)
{
    using namespace ::testing;
    EXPECT_CALL(*mock_registry, add_device(_)).Times(1);
    EXPECT_CALL(*mock_registry, remove_device(_)).Times(1);
    platform->start_monitor_devices(mock_event_handler_register, mock_registry);
    env.add_standard_device(GetParam());

    process_pending();

    remove_device();

    process_pending();
}

INSTANTIATE_TEST_CASE_P(EvdevPlatformHotplugging,
                        EvdevPlatformDeviceEvents,
                        ::testing::Values("synaptics-touchpad",
                                          "usb-keyboard",
                                          "usb-mouse",
                                          "laptop-keyboard",
                                          "bluetooth-magic-trackpad",
                                          "joystick-detection",
                                          "mt-screen-detection"
                                          ));
