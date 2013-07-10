/*
 * Copyright © 2012 Canonical Ltd.
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
 *              Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#include "src/server/input/android/android_input_manager.h"
#include "src/server/input/android/default_android_input_configuration.h"
#include "mir/input/event_filter.h"
#include "mir/input/cursor_listener.h"
#include "mir/input/input_targets.h"
#include "mir/input/null_input_report.h"

#include "mir_test/fake_shared.h"
#include "mir_test/fake_event_hub.h"
#include "mir_test/fake_event_hub_input_configuration.h"
#include "mir_test_doubles/mock_event_filter.h"
#include "mir_test/wait_condition.h"
#include "mir_test/event_factory.h"
#include "mir_test_doubles/mock_viewable_area.h"

#include <thread>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mi = mir::input;
namespace mia = mir::input::android;
namespace mis = mir::input::synthesis;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

using mtd::MockEventFilter;

namespace
{
using namespace ::testing;

struct StubInputTargets : public mi::InputTargets
{
    void for_each(std::function<void(std::shared_ptr<mi::InputChannel> const&)> const&)
    {
    }
};

struct MockCursorListener : public mi::CursorListener
{
    MOCK_METHOD2(cursor_moved_to, void(float, float));

    ~MockCursorListener() noexcept {}
};

struct AndroidInputManagerAndCursorListenerSetup : public testing::Test
{
    void SetUp()
    {
        static const geom::Rectangle visible_rectangle
        {
            geom::Point(),
            geom::Size{geom::Width(1024), geom::Height(1024)}
        };

        event_filter = std::make_shared<MockEventFilter>();
        configuration = std::make_shared<mtd::FakeEventHubInputConfiguration>(
            std::initializer_list<std::shared_ptr<mi::EventFilter> const>{event_filter},
            mt::fake_shared(viewable_area),
            mt::fake_shared(cursor_listener),
            std::make_shared<mi::NullInputReport>());

        ON_CALL(viewable_area, view_area())
            .WillByDefault(Return(visible_rectangle));

        fake_event_hub = configuration->the_fake_event_hub();

        input_manager = configuration->the_input_manager();

        stub_targets = std::make_shared<StubInputTargets>();
        configuration->set_input_targets(stub_targets);

        input_manager->start();
    }

    void TearDown()
    {
        input_manager->stop();
    }

    std::shared_ptr<mtd::FakeEventHubInputConfiguration> configuration;
    mia::FakeEventHub* fake_event_hub;
    std::shared_ptr<MockEventFilter> event_filter;
    NiceMock<mtd::MockViewableArea> viewable_area;
    std::shared_ptr<mi::InputManager> input_manager;
    MockCursorListener cursor_listener;
    std::shared_ptr<StubInputTargets> stub_targets;
};

}


TEST_F(AndroidInputManagerAndCursorListenerSetup, cursor_listener_receives_motion)
{
    using namespace ::testing;

    auto wait_condition = std::make_shared<mt::WaitCondition>();

    static const float x = 100.f;
    static const float y = 100.f;

    EXPECT_CALL(cursor_listener, cursor_moved_to(x, y)).Times(1);

    // The stack doesn't like shutting down while events are still moving through
    EXPECT_CALL(*event_filter, handle(_))
            .WillOnce(mt::ReturnFalseAndWakeUp(wait_condition));

    fake_event_hub->synthesize_builtin_cursor_added();
    fake_event_hub->synthesize_device_scan_complete();

    fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(x, y));

    wait_condition->wait_for_at_most_seconds(1);
}
