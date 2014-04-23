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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"

#include "mir_test_framework/stubbed_server_configuration.h"
#include "mir_test_framework/in_process_server.h"
#include "mir_test_framework/using_stub_client_platform.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test/spin_wait.h"

#include <gtest/gtest.h>

#include <atomic>

namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;
namespace mc = mir::compositor;

namespace
{

struct StubServerConfig : mtf::StubbedServerConfiguration
{
    std::shared_ptr<mir::graphics::Display> the_display()
    {
        return std::make_shared<mtd::NullDisplay>();
    }
};

class MirSurfaceSwapBuffersTest : public mir_test_framework::InProcessServer
{
public:
    mir::DefaultServerConfiguration& server_config() override { return server_config_; }

    StubServerConfig server_config_;
    mtf::UsingStubClientPlatform using_stub_client_platform;
};

void swap_buffers_callback(MirSurface*, void* ctx)
{
    auto buffers_swapped = static_cast<std::atomic<bool>*>(ctx);
    *buffers_swapped = true;
}

}

TEST_F(MirSurfaceSwapBuffersTest, swap_buffers_does_not_block_when_surface_is_not_composited)
{
    using namespace testing;

    auto const connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
    ASSERT_TRUE(mir_connection_is_valid(connection));

    MirSurfaceParameters request_params =
    {
        __PRETTY_FUNCTION__,
        640, 480,
        mir_pixel_format_abgr_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };

    auto const surface = mir_connection_create_surface_sync(connection, &request_params);
    ASSERT_NE(nullptr, surface);

    for (int i = 0; i < 10; ++i)
    {
        std::atomic<bool> buffers_swapped{false};

        mir_surface_swap_buffers(surface, swap_buffers_callback, &buffers_swapped);

        mir::test::spin_wait_for_condition_or_timeout(
            [&] { return buffers_swapped.load(); },
            std::chrono::seconds{5});

        /* 
         * ASSERT instead of EXPECT, since if we continue we will block in future
         * mir client calls (e.g mir_connection_release).
         */
        ASSERT_TRUE(buffers_swapped.load());
    }

    mir_surface_release_sync(surface);
    mir_connection_release(connection);
}
