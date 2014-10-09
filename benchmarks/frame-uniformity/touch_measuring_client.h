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

#ifndef TOUCH_MEASURING_CLIENT_H_
#define TOUCH_MEASURING_CLIENT_H_

#include <mir_toolkit/mir_client_library.h>

#include "mir_test/barrier.h"

#include <chrono>
#include <memory>
#include <vector>
#include <string>

class TouchMeasuringClient
{
public:
    TouchMeasuringClient(mir::test::Barrier &client_ready,
        mir::test::Barrier &client_done, 
        std::chrono::high_resolution_clock::duration const& touch_duration);
    
    class TestResults 
    {
    public:
        TestResults() = default;
        ~TestResults() = default;

        struct TouchSample
        {
            float x,y;
            std::chrono::high_resolution_clock::time_point event_time;
            std::chrono::high_resolution_clock::time_point frame_time;
        };
        std::vector<TouchSample> results();
        
        void record_frame_time(std::chrono::high_resolution_clock::time_point time);
        void record_pointer_coordinates(std::chrono::high_resolution_clock::time_point time,
                                        MirMotionPointer const& coordinates);
    private:
        std::vector<TouchSample> samples_being_prepared;
        std::vector<TouchSample> completed_samples;
    };
    
    void run(std::string const& connect_string);
    
    std::vector<TestResults::TouchSample> touch_samples();

private:
    mir::test::Barrier &client_ready;
    mir::test::Barrier &client_done;
    
    std::chrono::high_resolution_clock::duration const touch_duration;
    
    std::shared_ptr<TestResults> results;
};

#endif // TOUCH_MEASURING_CLIENT_H_
