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

#include "mir/input/touch_visualizer.h"

#include "mir_test/fake_event_hub.h"
#include "mir_test/barrier.h"
#include "mir_test/fake_shared.h"

#include "mir_test_framework/deferred_in_process_server.h"
#include "mir_test_framework/input_testing_server_configuration.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>
#include <functional>

namespace mi = mir::input;
namespace mis = mi::synthesis;
namespace mt = mir::test;
namespace geom = mir::geometry;
namespace mtf = mir_test_framework;

namespace
{

ACTION_P(UnblockBarrier, barrier)
{
    barrier->ready();
}

MATCHER(NoSpots, "")
{
    if (arg.size() > 0)
        return false;
    return true;
}

static bool
spots_have_position_and_pressure(std::vector<mi::TouchVisualizer::Spot> const& spots,
    std::vector<geom::Point> const& positions, float pressure)
{
    if (spots.size() != positions.size())
        return false;
    auto spots_it = spots.begin();
    auto positions_it = positions.begin();

    while (spots_it != spots.end())
    {
        auto const& spot = *spots_it;
        if (spot.touch_location != *positions_it)
            return false;
        if (spot.pressure != pressure)
            return false;
        spots_it++;
        positions_it++;
    }
    return true;
}

MATCHER_P(TouchedSpotsAt, positions, "")
{
    return spots_have_position_and_pressure(arg, positions, 1.0);
}

struct MockTouchVisualizer : public mi::TouchVisualizer
{
    MOCK_METHOD1(visualize_touches, void(std::vector<mi::TouchVisualizer::Spot> const&));
};

struct ServerConfiguration : mtf::InputTestingServerConfiguration
{
    mt::Barrier& test_complete;

    MockTouchVisualizer mock_visualizer;

    std::function<void(mtf::InputTestingServerConfiguration& server)> produce_events;
    std::function<void(MockTouchVisualizer& visualizer, mt::Barrier& test_complete)> expect_touchspots;

    static geom::Rectangle const display_bounds;    
    
    ServerConfiguration(mt::Barrier& test_complete)
        : InputTestingServerConfiguration({display_bounds}),
          test_complete(test_complete)
    {
    }
    
    std::shared_ptr<mi::TouchVisualizer> the_touch_visualizer() override
    {
        return mt::fake_shared(mock_visualizer);
    }
    
    void inject_input() override
    {
        produce_events(*this);
    }
    
    void establish_expectations()
    {
        // We need to do this before we start the input stack to avoid races
        // with the initial spot clearing.
        expect_touchspots(mock_visualizer, test_complete);
    }
};

geom::Rectangle const ServerConfiguration::display_bounds = {{0, 0}, {1600, 1600}};

struct TestTouchspotVisualizations : mtf::DeferredInProcessServer
{
    mt::Barrier test_complete_fence{2};
    ServerConfiguration server_configuration{test_complete_fence};

    mir::DefaultServerConfiguration& server_config() override { return server_configuration; }

    void start_server()
    {
        server_configuration.establish_expectations();

        DeferredInProcessServer::start_server();
        server_configuration.exec();
    }

    void TearDown()
    {
        test_complete_fence.ready();
        server_configuration.on_exit();
        DeferredInProcessServer::TearDown();
    }
};
    
geom::Point transform_to_screen_space(geom::Point in_touchpad_space)
{
    auto minimum_touch = mi::android::FakeEventHub::TouchScreenMinAxisValue;
    auto maximum_touch = mi::android::FakeEventHub::TouchScreenMaxAxisValue;
    
    auto display_width = ServerConfiguration::display_bounds.size.width.as_uint32_t();
    auto display_height = ServerConfiguration::display_bounds.size.height.as_uint32_t();
    
    float scale_x = float(display_width) / (maximum_touch - minimum_touch + 1);
    float scale_y = float(display_height) / (maximum_touch - minimum_touch + 1);
    
    return {scale_x * in_touchpad_space.x.as_uint32_t(), scale_y * in_touchpad_space.y.as_uint32_t()};
}

}

using namespace ::testing;

TEST_F(TestTouchspotVisualizations, touch_is_given_to_touchspot_visualizer)
{
    auto minimum_touch = mi::android::FakeEventHub::TouchScreenMinAxisValue;

    static geom::Point abs_touch = { minimum_touch, minimum_touch };
    static std::vector<geom::Point> expected_spots = { transform_to_screen_space(abs_touch) };
    
    server_configuration.produce_events = [&](mtf::InputTestingServerConfiguration& server)
        {
            server.fake_event_hub->synthesize_event(mis::a_touch_event().at_position(abs_touch));
            server.fake_event_hub->synthesize_event(mis::a_touch_event().at_position(abs_touch)
                .with_action(mis::TouchParameters::Action::Release));
        };
    server_configuration.expect_touchspots = [&](MockTouchVisualizer &visualizer, mt::Barrier& test_complete)
        {
            InSequence seq;

            // First we will see the spots cleared, as this is the start of a new gesture.
            EXPECT_CALL(visualizer, visualize_touches(NoSpots())).Times(1);
            EXPECT_CALL(visualizer, visualize_touches(TouchedSpotsAt(expected_spots)));
            EXPECT_CALL(visualizer, visualize_touches(NoSpots())).Times(1).
                WillOnce(UnblockBarrier(&test_complete));
        };
    start_server();
}

TEST_F(TestTouchspotVisualizations, touchspots_follow_gesture)
{
    auto minimum_touch = mi::android::FakeEventHub::TouchScreenMinAxisValue;
    auto maximum_touch = mi::android::FakeEventHub::TouchScreenMaxAxisValue;

    static geom::Point abs_touch = { minimum_touch, minimum_touch };
    static geom::Point abs_touch_2 = { maximum_touch, maximum_touch };
    static std::vector<geom::Point> expected_spots_1 =
       { transform_to_screen_space(abs_touch) };
    static std::vector<geom::Point> expected_spots_2 = 
       { transform_to_screen_space(abs_touch_2) };
    
    server_configuration.produce_events = [&](mtf::InputTestingServerConfiguration& server)
        {
            server.fake_event_hub->synthesize_event(mis::a_touch_event().at_position(abs_touch));
            server.fake_event_hub->synthesize_event(mis::a_touch_event().at_position(abs_touch_2)
                .with_action(mis::TouchParameters::Action::Move));
            server.fake_event_hub->synthesize_event(mis::a_touch_event().at_position(abs_touch_2)
                .with_action(mis::TouchParameters::Action::Release));
        };
    server_configuration.expect_touchspots = [&](MockTouchVisualizer &visualizer, mt::Barrier& test_complete)
        {
            InSequence seq;

            EXPECT_CALL(visualizer, visualize_touches(NoSpots())).Times(1);
            EXPECT_CALL(visualizer, visualize_touches(TouchedSpotsAt(expected_spots_1))).Times(1);
            EXPECT_CALL(visualizer, visualize_touches(TouchedSpotsAt(expected_spots_2))).Times(1);
            EXPECT_CALL(visualizer, visualize_touches(NoSpots())).
                Times(1).WillOnce(UnblockBarrier(&test_complete));
        };
    start_server();
}
