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

#include <assert.h>
#include <math.h>

#include <chrono>

#include <gtest/gtest.h>

namespace geom = mir::geometry;

namespace
{

geom::Point interpolated_touch_at_time(geom::Point touch_start, geom::Point touch_end,
                                       std::chrono::high_resolution_clock::time_point touch_start_time,
                                       std::chrono::high_resolution_clock::time_point touch_end_time,
                                       std::chrono::high_resolution_clock::time_point interpolated_touch_time)
{
    assert(interpolated_touch_time > touch_start_time);
    // std::chrono is a beautiful api
    double alpha = (interpolated_touch_time.time_since_epoch().count() - touch_start_time.time_since_epoch().count()) / static_cast<double>((touch_end_time.time_since_epoch().count() - touch_start_time.time_since_epoch().count()));
    
    auto ix = touch_start.x.as_int() + (touch_end.x.as_int()-touch_start.x.as_int())*alpha;
    auto iy = touch_start.y.as_int() + (touch_end.y.as_int()-touch_start.y.as_int())*alpha;
    return {ix, iy};
}

double compute_average_frame_offset(std::vector<TouchSamples::Sample> const& results,
                                    geom::Point touch_start_point, geom::Point touch_end_point,
                                    std::chrono::high_resolution_clock::time_point touch_start_time,
                                    std::chrono::high_resolution_clock::time_point touch_end_time)
{
    double sum = 0;
    int count = 0;
    for (auto const& sample : results)
    {
       auto expected_point = interpolated_touch_at_time(touch_start_point, touch_end_point, touch_start_time,
                                                        touch_end_time, sample.frame_time);
       auto dx = sample.x - expected_point.x.as_int();
       auto dy = sample.y - expected_point.y.as_int();
       auto distance = sqrt(dx*dx+dy*dy);
       sum += distance;
       count += 1;
    }
    return sum / count;
}

double compute_frame_uniformity(std::vector<TouchSamples::Sample> const& results,
    geom::Point touch_start_point, geom::Point touch_end_point,
    std::chrono::high_resolution_clock::time_point touch_start_time,
    std::chrono::high_resolution_clock::time_point touch_end_time)
{
    auto average_pixel_offset = compute_average_frame_offset(results, touch_start_point, touch_end_point,
        touch_start_time, touch_end_time);
    
    printf("Average pixel lag: %f \n", average_pixel_offset);
    
    double sum = 0;
    int count = 0;
    for (auto const& sample : results)
    {
       // TODO: Generalize
       auto expected_point = interpolated_touch_at_time(touch_start_point, touch_end_point, touch_start_time,
                                                        touch_end_time, sample.frame_time);
       auto dx = sample.x - expected_point.x.as_int();
       auto dy = sample.y - expected_point.y.as_int();
       auto distance = sqrt(dx*dx+dy*dy);

       sum += (distance-average_pixel_offset)*(distance-average_pixel_offset);
       count += 1;
    }
    return sqrt(sum/count);
}

}

// Main is inside a test to work around mtf issues
TEST(FrameUniformity, average_frame_offset)
{
    geom::Size const screen_size{1024, 1024};
    geom::Point const touch_start{0, 0};
    geom::Point const touch_end{1024, 1024};
    std::chrono::milliseconds touch_duration{2000};

    FrameUniformityTest t({screen_size, touch_start, touch_end, touch_duration});
    t.run_test();
  
    auto touch_timings = t.server_timings();
    auto touch_start_time = std::get<0>(touch_timings);
    auto touch_end_time = std::get<1>(touch_timings);
    auto samples = t.client_results()->get();
    printf("Samples: %d \n", samples.size());
    auto uniformity = compute_frame_uniformity(samples, touch_start, touch_end,
                                               touch_start_time, touch_end_time);
    printf("Frame Uniformity: %f \n", uniformity);
}
