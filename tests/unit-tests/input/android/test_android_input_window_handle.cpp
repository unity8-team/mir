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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "src/server/input/android/android_input_window_handle.h"

#include "mir/frontend/surface.h"
#include "mir/geometry/size.h"
#include "mir/input/input_channel.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_input_surface.h"

#include "mir/raii.h"

#include <cstdlib>
#include <cstring>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mi = mir::input;
namespace mia = mi::android;
namespace mf = mir::frontend;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{

struct StubInputApplicationHandle : public droidinput::InputApplicationHandle
{
    bool updateInfo() { return true; }
};

struct MockInputChannel : public mi::InputChannel
{
    MOCK_CONST_METHOD0(client_fd, int());
    MOCK_CONST_METHOD0(server_fd, int());
};

}

TEST(AndroidInputWindowHandle, update_info_uses_name_and_channel_from_surface)
{
    using namespace ::testing;

    std::string const testing_surface_name = "Test";

    int testing_server_fd;
    auto fd_wrapper = mir::raii::paired_calls([&testing_server_fd]()
        {
            // We need a real open fd, as InputWindowHandle's constructor will fcntl() it, and
            // InputWindowHandle's destructor will close() it.
            char *filename = strdup("/tmp/mir_unit_test_XXXXXX");
            testing_server_fd = mkstemp(filename);
            // We don't actually need the file to exist after this test.
            unlink(filename);
            free(filename);
        },
        [&testing_server_fd]()
        {
            if (testing_server_fd > 0) close(testing_server_fd);
        });

    MockInputChannel mock_channel;
    mtd::MockInputSurface mock_surface;

    EXPECT_CALL(mock_channel, server_fd())
        .Times(1)
        .WillOnce(Return(testing_server_fd));

    EXPECT_CALL(mock_surface, name())
        .Times(1)
        .WillOnce(Return(testing_surface_name));

    mia::InputWindowHandle handle(new StubInputApplicationHandle(),
                                  mt::fake_shared(mock_channel), mt::fake_shared(mock_surface));

    auto info = handle.getInfo();

    EXPECT_EQ(droidinput::String8(testing_surface_name.c_str()), info->name);

    EXPECT_EQ(testing_server_fd, info->inputChannel->getFd());
}
