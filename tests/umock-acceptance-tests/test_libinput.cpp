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

#include "mir/input/platform.h"
#include "mir/shared_library.h"
#include "mir/options/option.h"

#include "mir_test_framework/udev_environment.h"
#include "mir_test_framework/executable_path.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mtf = mir_test_framework;
namespace mo = mir::options;
using namespace ::testing;
namespace
{

struct MockOption : mo::Option
{
    MOCK_CONST_METHOD1(is_set, bool(char const*));
    MOCK_CONST_METHOD2(get, bool(char const*, bool));
    MOCK_CONST_METHOD2(get, int(char const*, int));
    MOCK_CONST_METHOD2(get, std::string(char const* name, char const*));
    MOCK_CONST_METHOD1(get, boost::any const&(char const*));
};

auto get_libinput_platform()
{
    auto path = mtf::server_input_platform("input-evdev");
    return std::make_shared<mir::SharedLibrary>(path);
}

char const probe_input_platform_symbol[] = "probe_input_platform";
char const host_socket_opt[] = "host-socket";

}

TEST(LibInput, DISABLED_probes_as_unsupported_without_device_access)
{
    NiceMock<MockOption> options;

    // dumb assumption - nobody runs this test cases as root..
    // or allows accessing evdev input devices from non privileged users.
    auto library = get_libinput_platform();
    auto probe_fun = library->load_function<mir::input::ProbePlatform>(probe_input_platform_symbol);
    EXPECT_THAT(probe_fun(options), Eq(mir::input::PlatformPriority::unsupported));
}

TEST(LibInput, probes_as_supported_with_at_least_one_device_to_deal_with)
{
    mtf::UdevEnvironment env;
    env.add_standard_device("laptop-keyboard");
    NiceMock<MockOption> options;

    auto library = get_libinput_platform();
    auto probe_fun = library->load_function<mir::input::ProbePlatform>(probe_input_platform_symbol);
    EXPECT_THAT(probe_fun(options), Ge(mir::input::PlatformPriority::supported));
}

TEST(LibInput, probes_as_unsupported_on_nested_configs)
{
    mtf::UdevEnvironment env;
    env.add_standard_device("laptop-keyboard");
    NiceMock<MockOption> options;
    ON_CALL(options,is_set(StrEq(host_socket_opt)))
        .WillByDefault(Return(true));
    ON_CALL(options,get(StrEq(host_socket_opt), Matcher<char const*>(_)))
        .WillByDefault(Return("something"));

    auto library = get_libinput_platform();
    auto probe_fun = library->load_function<mir::input::ProbePlatform>(probe_input_platform_symbol);
    EXPECT_THAT(probe_fun(options), Eq(mir::input::PlatformPriority::unsupported));
}

TEST(LibInput, probes_as_supported_when_umock_dev_available_or_before_input_devices_are_available)
{
    mtf::UdevEnvironment env;
    NiceMock<MockOption> options;

    auto library = get_libinput_platform();
    auto probe_fun = library->load_function<mir::input::ProbePlatform>(probe_input_platform_symbol);
    EXPECT_THAT(probe_fun(options), Eq(mir::input::PlatformPriority::supported));
}
