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

#include "src/platform/input/evdev/libinput_device.h"
#include "src/platform/input/evdev/libinput_wrapper.h"

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

class StubEventSink : public mi::EventSink
{
public:
    void handle_input(MirEvent const&) override {}
};

struct LibInputDevice : public ::testing::Test
{
    mir::test::doubles::MockLibInput mock_libinput;
    mtd::MockInputEventHandlerRegister mock_registry;
    StubEventSink stub_sink;
    std::shared_ptr<mie::LibInputWrapper> wrapper;

    libinput* fake_input = reinterpret_cast<libinput*>(0xF4C3);
    libinput_device* fake_device = reinterpret_cast<libinput_device*>(0xF4C4);

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
    dev.start(mock_registry, stub_sink);
}

TEST_F(LibInputDevice, stop_unrefs_libinput_device)
{
    using namespace ::testing;
    char const* path = "/path/to/dev";
    EXPECT_CALL(mock_libinput, libinput_device_unref(fake_device))
        .Times(1);
    mie::LibInputDevice dev(wrapper, path);
    dev.start(mock_registry, stub_sink);
    dev.stop(mock_registry);
}
