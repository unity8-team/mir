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

#include "touch_measuring_client.h"

#include "mir/time/clock.h"

#include "mir_toolkit/mir_client_library.h"

#include <chrono>
#include <memory>
#include <vector>

#include <assert.h>

namespace mtf = mir_test_framework;

void TouchMeasuringClient::TestResults::record_frame_time(std::chrono::high_resolution_clock::time_point time)
{
    for (auto &sample: samples_being_prepared)
    {
        sample.frame_time = time;
        completed_samples.push_back(std::move(sample));
    }
    samples_being_prepared.clear();
}
    
void TouchMeasuringClient::TestResults::record_pointer_coordinates(std::chrono::high_resolution_clock::time_point time,
    MirMotionPointer const& coordinates)
{
    samples_being_prepared.push_back(TouchSample{coordinates.x, coordinates.y, time, {}});
}

// TODO: Use rvalue optimization
std::vector<TouchMeasuringClient::TestResults::TouchSample> TouchMeasuringClient::TestResults::results()
{
    return completed_samples;
}

namespace
{

MirSurface *create_surface(MirConnection *connection)
{
    MirPixelFormat pixel_format;
    unsigned int valid_formats;
    mir_connection_get_available_surface_formats(connection, &pixel_format, 1, &valid_formats);
    MirSurfaceParameters const surface_params = { "frame-uniformity-test",
        0, 0, /* Allow the server to choose a size for us */
        pixel_format,
        mir_buffer_usage_hardware, 
        mir_display_output_id_invalid};
    
    auto surface = mir_connection_create_surface_sync(connection, &surface_params);
    assert(mir_surface_is_valid(surface));

    return surface;
}

void input_callback(MirSurface * /* surface */, MirEvent const* event, void* context)
{
    auto results = static_cast<TouchMeasuringClient::TestResults*>(context);
    
    if (event->type != mir_event_type_motion)
        return;
    
    auto const& mev = event->motion;
    if (mev.action != mir_motion_action_down &&
        mev.action != mir_motion_action_up &&
        mev.action != mir_motion_action_move)
    {
        return;
    }

    // We could support multitouch
    results->record_pointer_coordinates(std::chrono::high_resolution_clock::now(), mev.pointer_coordinates[0]);
}

void collect_input_and_frame_timing(MirSurface *surface, mtf::CrossProcessSync &client_ready, std::chrono::high_resolution_clock::duration duration, std::shared_ptr<TouchMeasuringClient::TestResults> results)
{
    MirEventDelegate event_handler = { input_callback, results.get() };
    mir_surface_set_event_handler(surface, &event_handler);
    
    client_ready.signal_ready();

    auto now = []() { return std::chrono::high_resolution_clock::now(); };

    // May be better if end time were relative to the first input event
    auto end_time = now() + duration;
    while (now() < end_time)
    {
        mir_surface_swap_buffers_sync(surface);
        results->record_frame_time(now());
    }
}

}

TouchMeasuringClient::TouchMeasuringClient(mtf::CrossProcessSync &client_ready,
    mtf::CrossProcessSync &client_done, std::chrono::high_resolution_clock::duration const& touch_duration)
    : client_ready(client_ready),
      client_done(client_done),
      touch_duration(touch_duration),
      results(std::make_shared<TouchMeasuringClient::TestResults>())
{
}

void TouchMeasuringClient::run(std::string const& connect_string)
{
    auto connection = mir_connect_sync(connect_string.c_str(), "frame-uniformity-test");
    assert(mir_connection_is_valid(connection));
    
    auto surface = create_surface(connection);

    collect_input_and_frame_timing(surface, client_ready, touch_duration, results);
    
    mir_surface_release_sync(surface);
    mir_connection_release(connection);

    client_done.signal_ready();
}

std::vector<TouchMeasuringClient::TestResults::TouchSample> TouchMeasuringClient::touch_samples()
{
    return results->results();
}
