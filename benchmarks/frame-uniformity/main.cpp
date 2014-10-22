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

double pixel_lag_for_sample_at_time(geom::Point touch_start_point, geom::Point touch_end_point,
    std::chrono::high_resolution_clock::time_point touch_start_time,
    std::chrono::high_resolution_clock::time_point touch_end_time,
    TouchSamples::Sample const& sample)
{
    auto expected_point = interpolated_touch_at_time(touch_start_point, touch_end_point, touch_start_time,
        touch_end_time, sample.frame_time);
    auto dx = sample.x - expected_point.x.as_int();
    auto dy = sample.y - expected_point.y.as_int();
    auto distance = sqrt(dx*dx+dy*dy);
    return distance;
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
        auto distance = pixel_lag_for_sample_at_time(touch_start_point, touch_end_point, touch_start_time, 
            touch_end_time, sample);
        sum += distance;
        count += 1;
    }
    return sum / count;
}

struct Results
{
    double average_pixel_offset;
    double frame_uniformity;
};

Results compute_frame_uniformity(std::vector<TouchSamples::Sample> const& results,
    geom::Point touch_start_point, geom::Point touch_end_point,
    std::chrono::high_resolution_clock::time_point touch_start_time,
    std::chrono::high_resolution_clock::time_point touch_end_time)
{
    auto average_pixel_offset = compute_average_frame_offset(results, touch_start_point, touch_end_point,
        touch_start_time, touch_end_time);
    
    double sum = 0;
    int count = 0;
    for (auto const& sample : results)
    {
        auto distance = pixel_lag_for_sample_at_time(touch_start_point, touch_end_point, touch_start_time, 
            touch_end_time, sample);
        sum += (distance-average_pixel_offset)*(distance-average_pixel_offset);
        count += 1;
    }
    double uniformity = sqrt(sum/count);
    return {average_pixel_offset, uniformity};
}

}

// Main is inside a test to work around mir_test_framework 'issues' (e.g. mir_test_framework contains
// a main function).
TEST(FrameUniformity, average_frame_offset)
{
    geom::Size const screen_size{1024, 1024};
    geom::Point const touch_start_point{0, 0};
    geom::Point const touch_end_point{1024, 1024};
    std::chrono::milliseconds touch_duration{500};
    
    int const run_count = 1;
    double average_lag = 0, average_uniformity = 0;

    for (int i = 0; i < run_count; i++)
    {
        FrameUniformityTest t({screen_size, touch_start_point, touch_end_point, touch_duration});

        t.run_test();
  
        auto touch_timings = t.server_timings();
        auto touch_start_time = std::get<0>(touch_timings);
        auto touch_end_time = std::get<1>(touch_timings);
        auto samples = t.client_results()->get();

        auto results = compute_frame_uniformity(samples, touch_start_point, touch_end_point,
            touch_start_time, touch_end_time);
        
        average_lag += results.average_pixel_offset;
        average_uniformity += results.frame_uniformity;
    }
    
    average_lag /= run_count;
    average_uniformity /= run_count;
    
    printf("Average pixel lag: %f \n", average_lag);
    printf("Frame Uniformity: %f \n", average_uniformity);
}
