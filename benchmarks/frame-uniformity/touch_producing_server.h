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

#ifndef TOUCH_PRODUCING_SERVER_H_
#define TOUCH_PRODUCING_SERVER_H_

#include "mir_test_framework/fake_event_hub_server_configuration.h"
#include "mir_test/barrier.h"

#include "mir/geometry/rectangle.h"
#include "mir/geometry/point.h"

#include <thread>

class TouchProducingServer : public mir_test_framework::FakeEventHubServerConfiguration
{
public:
    TouchProducingServer(mir::geometry::Rectangle screen_dimensions, mir::geometry::Point touch_start, mir::geometry::Point touch_end, std::chrono::high_resolution_clock::duration touch_duration, mir::test::Barrier &client_ready);
    
    std::tuple<std::chrono::high_resolution_clock::time_point,std::chrono::high_resolution_clock::time_point>
        touch_timings();

    ~TouchProducingServer();    
private:
    mir::geometry::Rectangle const screen_dimensions;

    mir::geometry::Point const touch_start;
    mir::geometry::Point const touch_end;
    std::chrono::high_resolution_clock::duration const touch_duration;

    mir::test::Barrier &client_ready;
    
    std::thread input_injection_thread;
    
    std::chrono::high_resolution_clock::time_point touch_start_time;
    std::chrono::high_resolution_clock::time_point touch_end_time;
    
    void synthesize_event_at(mir::geometry::Point const& point);
    void thread_function();
};

#endif
