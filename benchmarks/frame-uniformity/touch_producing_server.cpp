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
#include "vsync_simulating_graphics_platform.h"

#include "mir_test/event_factory.h"
#include "mir_test/fake_event_hub.h"

#include <functional>

namespace mi = mir::input;
namespace mia = mi::android;
namespace mg = mir::graphics;
namespace mis = mi::synthesis;
namespace geom = mir::geometry;
namespace mt = mir::test;

namespace mtf = mir_test_framework;

TouchProducingServer::TouchProducingServer(geom::Rectangle screen_dimensions, geom::Point touch_start,
    geom::Point touch_end, std::chrono::high_resolution_clock::duration touch_duration,
    mt::Barrier &client_ready)
    : FakeEventHubServerConfiguration({screen_dimensions}),
      screen_dimensions(screen_dimensions),
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

std::shared_ptr<mg::Platform> TouchProducingServer::the_graphics_platform()
{
    // TODO: Fix
    int const refresh_rate_in_hz = 60;

    if (!graphics_platform)
        graphics_platform = std::make_shared<VsyncSimulatingPlatform>(screen_dimensions.size, refresh_rate_in_hz);
    
    return graphics_platform;
}

// This logic limits us to supporting screens at 0,0
void TouchProducingServer::synthesize_event_at(geom::Point const& point)
{
    auto const minimum_touch = mia::FakeEventHub::TouchScreenMinAxisValue;
    auto const maximum_touch = mia::FakeEventHub::TouchScreenMaxAxisValue;
    auto const display_width = screen_dimensions.size.width.as_int();
    auto const display_height = screen_dimensions.size.height.as_int();
    
    auto px_frac = point.x.as_int() / static_cast<double>(display_width);
    auto py_frac = point.y.as_int() / static_cast<double>(display_height);
    auto const abs_touch_x = minimum_touch + (maximum_touch-minimum_touch) * px_frac;
    auto const abs_touch_y = minimum_touch + (maximum_touch-minimum_touch) * py_frac;
    
    fake_event_hub->synthesize_event(
        mis::a_touch_event().at_position({abs_touch_x, abs_touch_y}));                                      
}

void TouchProducingServer::thread_function()
{
    // We could make the touch sampling rate customizable
    std::chrono::milliseconds const pause_between_events{10};

    client_ready.ready();
    
    auto start = std::chrono::high_resolution_clock::now();
    auto end = start + touch_duration;
    auto now = start;

    // We could tighten the touch start and end times further.
    touch_start_time = start;
    while (now < end)
    {
        std::this_thread::sleep_for(pause_between_events);

        now = std::chrono::high_resolution_clock::now();
        touch_end_time = now;
        
        double alpha = (now.time_since_epoch().count()-start.time_since_epoch().count()) / static_cast<double>(end.time_since_epoch().count()-start.time_since_epoch().count());
        auto point = geom::Point{touch_start.x.as_int()+(touch_end.x.as_int()-touch_start.x.as_int())*alpha,
            touch_start.y.as_int()+(touch_end.y.as_int()-touch_start.y.as_int())*alpha};
        synthesize_event_at(point);
    }
}

std::tuple<std::chrono::high_resolution_clock::time_point,std::chrono::high_resolution_clock::time_point>
TouchProducingServer::touch_timings()
{
    return std::tuple<std::chrono::high_resolution_clock::time_point,std::chrono::high_resolution_clock::time_point>{touch_start_time, touch_end_time};
}
