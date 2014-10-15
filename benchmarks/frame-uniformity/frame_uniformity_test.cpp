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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "frame_uniformity_test.h"

FrameUniformityTest::FrameUniformityTest(FrameUniformityTestParameters const& parameters)
    : client_ready_fence{2},
      client_done_fence{2},
      server_configuration({{0, 0}, parameters.screen_size},
                           parameters.touch_start,
                           parameters.touch_end,
                           parameters.touch_duration,
                           client_ready_fence),
      client(client_ready_fence, client_done_fence, parameters.touch_duration)
{
}

FrameUniformityTest::~FrameUniformityTest()
{
}

mir::DefaultServerConfiguration& FrameUniformityTest::server_config()
{
    return server_configuration;
}

void FrameUniformityTest::run_test()
{
    start_server();
    
    std::thread client_thread([&]()
    {
        client.run(new_connection());
    });

    // TODO: Client done fence necessary?
    client_done_fence.ready();
    if (client_thread.joinable())
        client_thread.join();

    stop_server();
}

std::shared_ptr<TouchSamples> FrameUniformityTest::client_results()
{
    return client.results();
}

std::tuple<std::chrono::high_resolution_clock::time_point,std::chrono::high_resolution_clock::time_point> FrameUniformityTest::server_timings()
{
    return server_configuration.touch_timings();
}
