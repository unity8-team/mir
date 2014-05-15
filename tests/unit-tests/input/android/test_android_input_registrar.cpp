/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir/input/surface.h"

#include "src/server/input/android/android_input_registrar.h"

#include "mir_test_doubles/mock_android_input_dispatcher.h"
#include "mir_test_doubles/stub_scene_surface.h"

#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <stdexcept>

namespace mi = mir::input;
namespace mia = mi::android;
namespace ms = mir::scene;
namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace geom = mir::geometry;

namespace
{

// TODO: It would be nice if it were possible to mock the interface between
// droidinput::InputChannel and droidinput::InputDispatcher rather than use
// valid fds to allow non-throwing construction of a real input channel.
struct AndroidInputRegistrarFdSetup : public testing::Test
{
    AndroidInputRegistrarFdSetup()
        : surface(socket(AF_UNIX, SOCK_SEQPACKET, 0))
    {
    }
    ~AndroidInputRegistrarFdSetup()
    {
        close(surface.fd);
    }
    std::shared_ptr<mtd::MockAndroidInputDispatcher> dispatcher = std::make_shared<mtd::MockAndroidInputDispatcher>();
    mtd::StubSceneSurface surface;
};

MATCHER_P(WindowHandleFor, channel, "")
{
    if (arg->getInputChannel()->getFd() != channel->server_fd())
        return false;
    return true;
}

}

TEST_F(AndroidInputRegistrarFdSetup, input_channel_opened_behavior)
{
    using namespace ::testing;

    EXPECT_CALL(*dispatcher, registerInputChannel(_, WindowHandleFor(surface.input_channel()), _)).Times(1)
        .WillOnce(Return(droidinput::OK));

    mia::InputRegistrar registrar(dispatcher);

    registrar.surface_added(&surface);
    EXPECT_THROW({
            // We can't open a surface twice
            registrar.surface_added(&surface);
    }, std::logic_error);
}

TEST_F(AndroidInputRegistrarFdSetup, input_channel_closed_behavior)
{
    using namespace ::testing;

    EXPECT_CALL(*dispatcher, registerInputChannel(_, WindowHandleFor(surface.input_channel()), _)).Times(1)
        .WillOnce(Return(droidinput::OK));
    EXPECT_CALL(*dispatcher, unregisterInputChannel(_)).Times(1);
    mia::InputRegistrar registrar(dispatcher);

    EXPECT_THROW({
            // We can't close a surface which hasn't been opened
            registrar.surface_removed(&surface);
    }, std::logic_error);
    registrar.surface_added(&surface);
    registrar.surface_removed(&surface);
    EXPECT_THROW({
            // Nor can we close a surface twice
            registrar.surface_removed(&surface);
    }, std::logic_error);
}

TEST_F(AndroidInputRegistrarFdSetup, monitor_flag_is_passed_to_dispatcher)
{
    using namespace ::testing;

    surface.set_reception_mode(mi::InputReceptionMode::receives_all_input);

    EXPECT_CALL(*dispatcher, registerInputChannel(_, _, true)).Times(1)
        .WillOnce(Return(droidinput::OK));

    mia::InputRegistrar registrar(dispatcher);

    registrar.surface_added(&surface);
}
