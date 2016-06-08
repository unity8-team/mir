/*
 * Copyright © 2016 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"
#include "mir_test_framework/connected_client_headless_server.h"
#include "mir/logging/logger.h"
#include "mir/test/fake_shared.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <sstream>

using namespace testing;
using namespace mir_test_framework;

namespace
{
class StringStreamLogger : public mir::logging::Logger
{
public:
    void log(mir::logging::Severity, std::string const& message, std::string const& component) override
    {
        out << "[StringStreamLogger] " << component << ": " << message << std::endl;
    }

    std::stringstream out;
};

struct ClientLogging : ConnectedClientHeadlessServer
{
    StringStreamLogger logger;

    void SetUp() override
    {
        add_to_environment("MIR_CLIENT_PERF_REPORT", "log");
        ConnectedClientHeadlessServer::SetUp();
        mir::logging::set_logger(mir::test::fake_shared(logger));
    }
};
} // namespace

TEST_F(ClientLogging, reports_performance)
{
    auto spec = mir_connection_create_spec_for_normal_surface(
                   connection, 123, 456, mir_pixel_format_abgr_8888);
    ASSERT_THAT(spec, NotNull());
    mir_surface_spec_set_name(spec, "Rumpelstiltskin");
    mir_surface_spec_set_buffer_usage(spec, mir_buffer_usage_software);
    auto surf = mir_surface_create_sync(spec);
    ASSERT_THAT(surf, NotNull());
    mir_surface_spec_release(spec);

    int const target_fps = 10;
    int const nseconds = 3;
    auto const target_render_time = std::chrono::milliseconds(3);
    auto bs = mir_surface_get_buffer_stream(surf);
    for (int s = 0; s < nseconds; ++s)
    {
        for (int f = 0; f < target_fps; ++f)
        {
            MirGraphicsRegion region;
            // Performance report sees this as the start of a frame:
            mir_buffer_stream_get_graphics_region(bs, &region);
            std::this_thread::sleep_for(target_render_time);
            // Performance report sees this as the end of a frame:
            mir_buffer_stream_swap_buffers_sync(bs);
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    int reports = 0;
    while (!logger.out.eof())
    {
        std::string line;
        std::getline(logger.out, line);
        auto perf = line.find(" perf: ");
        if (perf != line.npos)
        {
            ++reports;
            char name[256];
            float fps, render, lag;
            int nbuffers;
            int fields = sscanf(line.c_str() + perf,
                " perf: %255[^:]: %f FPS, render time %fms, buffer lag %fms "
                " (%d buffers)",
                name, &fps, &render, &lag, &nbuffers);

            ASSERT_THAT(fields, Eq(5)) << "Log line = {" << line << "}";

            EXPECT_STREQ("Rumpelstiltskin", name);
            EXPECT_THAT(render, Gt(target_render_time.count() - 1));

            // On a regular machine, render will be 3 (milliseconds) matching
            // the above render loop. CI however is super-slow at times so
            // we just check rendering completed in under one second.
            // That's still easily enough to regression-test LP: #1581368
            // where it had values in the millions.
            EXPECT_THAT(render, Lt(1000));

            EXPECT_THAT(lag, Gt(0.0f));

            // In order to see all three buffers the client must be rendering
            // at least in bursts faster than the compositor consumes them.
            // On the surface of it the above render loop should appear to
            // do that, but in reality we're feeding an unthottled fake
            // compositor here so may never hit all three buffers...
            EXPECT_THAT(nbuffers, Ge(2));

            ASSERT_FALSE(Test::HasFailure()) << "Log line = {" << line << "}";
        }
    }

    EXPECT_THAT(reports, Ge(nseconds-1));

    mir_surface_release_sync(surf);
}
