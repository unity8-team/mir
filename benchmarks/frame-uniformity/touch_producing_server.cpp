/*
 * Copyright © 2013-2014 Canonical Ltd.
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

#include "touch_producing_server.h"

#include <functional>

namespace geom = mir::geometry;
namespace mtf = mir_test_framework;

TouchProducingServer::TouchProducingServer(geom::Rectangle screen_dimensions, geom::Point touch_start,
    geom::Point touch_end, std::chrono::high_resolution_clock::duration touch_duration,
    mtf::CrossProcessSync &client_ready)
    : FakeEventHubServerConfiguration({screen_dimensions}),
      touch_start(touch_start),
      touch_end(touch_end),
      touch_duration(touch_duration),
      client_ready(client_ready)
{
    input_injection_thread = std::thread(std::mem_fn(&TouchProducingServer::thread_function), this);
}                                           

TouchProducingServer::~TouchProducingServer()
{
    if (input_injection_thread.joinable())
        input_injection_thread.join();
}

void TouchProducingServer::synthesize_event_at(geom::Point const& point)
{
    // TODO
    (void) point;
}

void TouchProducingServer::thread_function()
{
    // TODO: Hack
    std::chrono::milliseconds const pause_between_events{10};

    client_ready.wait_for_signal_ready_for();
    
    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + touch_duration;
    auto now = start;

    // TODO: Tighten touch start and end times?
    touch_start_time = start;
    while (now < start)
    {
        std::this_thread::sleep_for(pause_between_events);

        now = std::chrono::high_resolution_clock::now();
        
        double alpha = (now.time_since_epoch().count()-start.time_since_epoch().count()) / static_cast<double>(end.time_since_epoch().count()-start.time_since_epoch().count());
        auto point = geom::Point{touch_start.x.as_int()+(touch_end.x.as_int()-touch_start.x.as_int())*alpha,
            touch_start.y.as_int()+(touch_end.y.as_int()-touch_start.y.as_int())*alpha};
        synthesize_event_at(point);
    }
    touch_end_time = std::chrono::high_resolution_clock::now();
}

std::tuple<std::chrono::high_resolution_clock::time_point,std::chrono::high_resolution_clock::time_point>
TouchProducingServer::touch_timings()
{
    return std::tuple<std::chrono::high_resolution_clock::time_point,std::chrono::high_resolution_clock::time_point>{touch_start_time, touch_end_time};
}
