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

#include "mir/udev/wrapper.h"

#include "mir_test_doubles/mock_main_loop.h"
#include "mir_test_doubles/mock_input_device_registry.h"
#include "mir_test_doubles/mock_input_multiplexer.h"

#include <gmock/gmock.h>


namespace mi = mir::input;
namespace mie = mi::evdev;
namespace mr = mir::report;
namespace mtd = mir::test::doubles;

struct EvdevPlatform : ::testing::Test
{
public:
    EvdevPlatform()
        : platform(mr::null_input_report(), ctx, monitor)
    {
    }
    mir::udev::Context * ctx = new mir::udev::Context;
    mir::udev::Monitor * monitor = new mir::udev::Monitor{*ctx};
    mie::Platform platform;
    ::testing::NiceMock<mtd::MockMultiplexer> mock_multiplexer;
    std::shared_ptr<::testing::NiceMock<mtd::MockInputDeviceRegistry>> mock_registry =
        std::make_shared<::testing::NiceMock<mtd::MockInputDeviceRegistry>>();
};

TEST_F(EvdevPlatform, registers_to_multiplexer_on_start)
{
    using namespace ::testing;
    EXPECT_CALL(mock_multiplexer, register_fd_handler(_,_,_));
    platform.start_monitor_devices(mock_multiplexer, mock_registry);
}

TEST_F(EvdevPlatform, unregisters_to_multiplexer_on_stop)
{
    using namespace ::testing;
    EXPECT_CALL(mock_multiplexer, unregister_fd_handler(_));
    platform.stop_monitor_devices(mock_multiplexer);
}
