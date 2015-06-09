/* Copyright© 2015 Canonical Ltd.
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

#include "src/server/input/touch_stream_rewriter.h"

#include "mir/events/event_private.h"
#include "mir/events/event_builders.h"

#include "mir_test/fake_shared.h"
#include "mir_test/event_matchers.h"
#include "mir_test_doubles/mock_input_dispatcher.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

// TODO: Remove
#include "mir/event_printer.h"
#include <iostream>

namespace mi = mir::input;
namespace mev = mir::events;
namespace mt = mir::test;
namespace mtd = mt::doubles;

using namespace ::testing;


namespace mir
{
// TODO: Remove
struct PrintingDispatcher : public mtd::MockInputDispatcher
{
    void dispatch(MirEvent const& ev) override
    {
        std::cout << "Stuff: " << ev << std::endl;
    }
};
}

namespace
{

struct TouchStreamRewriter : public ::testing::Test
{
    TouchStreamRewriter()
        : rewriter(mt::fake_shared(next_dispatcher))
    {
    }
    
    mtd::MockInputDispatcher next_dispatcher;
    //    mir::PrintingDispatcher next_dispatcher;
    mi::TouchStreamRewriter rewriter;
};

void add_another_touch(mir::EventUPtr const& ev, MirTouchId id, MirTouchAction action)
{
    mev::add_touch(*ev, id, action, mir_touch_tooltype_finger,
                   0, 0, 0, 0, 0, 0);
}
    
mir::EventUPtr make_touch(MirTouchId id, MirTouchAction action)
{
    auto ev = mev::make_event(MirInputDeviceId(0), std::chrono::nanoseconds(0),
                              mir_input_event_modifier_none);
    add_another_touch(ev, id, action);
    return ev;
}

}

// TODO: Should we test that actions are split? The thing is with the
// way the action mask works now they have to be...

// We make a touch that represents two unseen touch ID's changing
// this way we expect the server to generate two seperate events to first
// report the down actions
TEST_F(TouchStreamRewriter, missing_touch_downs_are_inserted)
{
    auto touch = make_touch(0, mir_touch_action_change);
    add_another_touch(touch, 1, mir_touch_action_change);

    auto expected_ev_1 = make_touch(0, mir_touch_action_down);
    auto expected_ev_2 = make_touch(0, mir_touch_action_change);
    add_another_touch(expected_ev_2, 1, mir_touch_action_down);

    InSequence seq;
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_1)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_2)));

    // DO we really want this?
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*touch)));
    
    rewriter.dispatch(*touch);
}

TEST_F(TouchStreamRewriter, multiple_missing_touch_downs_are_inserted)
{
    auto touch = make_touch(0, mir_touch_action_down);

    // We dispatch touch 2 with 2 touches (1 and 2) which never went down
    auto touch_2 = make_touch(0, mir_touch_action_change);
    add_another_touch(touch_2, 1, mir_touch_action_change);
    add_another_touch(touch_2, 2, mir_touch_action_change);

    auto const& expected_ev_1 = touch;
    auto expected_ev_2 = make_touch(0, mir_touch_action_change);
    add_another_touch(expected_ev_2, 1, mir_touch_action_down);
    auto expected_ev_3 = make_touch(0, mir_touch_action_change);
    add_another_touch(expected_ev_3, 1, mir_touch_action_change);
    add_another_touch(expected_ev_3, 2, mir_touch_action_down);
    auto const& expected_ev_4 = touch_2;

    InSequence seq;
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_1)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_2)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_3)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_4)));

    rewriter.dispatch(*touch);
    rewriter.dispatch(*touch_2);
}

// In this case we first put two touches down, then we show an event which
// reports one of them changing without the others, in this case we
// must insert a touch up event for the ID which has gone missing.
TEST_F(TouchStreamRewriter, missing_touch_releases_are_inserted)
{
    auto touch_1 = make_touch(0, mir_touch_action_down);
    auto touch_2 = make_touch(0, mir_touch_action_change);
    add_another_touch(touch_2, 1, mir_touch_action_down);
    auto touch_3 = make_touch(0, mir_touch_action_change);

    auto expected_release_insert = make_touch(0, mir_touch_action_change);
    add_another_touch(expected_release_insert, 1, mir_touch_action_up);

    InSequence seq;
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*touch_1)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*touch_2)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_release_insert)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*touch_3)));

    rewriter.dispatch(*touch_1);
    rewriter.dispatch(*touch_2);
    rewriter.dispatch(*touch_3);
}

TEST_F(TouchStreamRewriter, multiple_missing_releases_are_inserted)
{
    auto touch_1 = make_touch(0, mir_touch_action_down);
    auto touch_2 = make_touch(0, mir_touch_action_change);
    add_another_touch(touch_2, 1, mir_touch_action_down);
    auto touch_3 = make_touch(0, mir_touch_action_change);
    add_another_touch(touch_3, 1, mir_touch_action_change);
    add_another_touch(touch_3, 2, mir_touch_action_down);
    auto touch_4 = make_touch(2, mir_touch_action_change);

    auto const& expected_ev_1 = touch_1;
    auto const& expected_ev_2 = touch_2;
    auto const& expected_ev_3 = touch_3;
    auto expected_ev_4 = make_touch(0, mir_touch_action_up);
    add_another_touch(touch_2, 1, mir_touch_action_change);
    add_another_touch(touch_2, 2, mir_touch_action_down);
    auto expected_ev_5 = make_touch(1, mir_touch_action_up);
    add_another_touch(touch_2, 2, mir_touch_action_change);
    auto const& expected_ev_6 = touch_4;

    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_1)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_2)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_3)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_4)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_5)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_6)));

    rewriter.dispatch(*touch_1);
    rewriter.dispatch(*touch_2);
    rewriter.dispatch(*touch_3);
    rewriter.dispatch(*touch_4);
}

TEST_F(TouchStreamRewriter, missing_up_and_down_is_inserted)
{
    auto touch_1 = make_touch(0, mir_touch_action_down);
    auto touch_2 = make_touch(0, mir_touch_action_change);
    add_another_touch(touch_2, 1, mir_touch_action_down);
    auto touch_3 = make_touch(1, mir_touch_action_change);
    add_another_touch(touch_3, 2, mir_touch_action_change);

    auto const& expected_ev_1 = touch_1;
    auto const& expected_ev_2 = touch_2;
    auto expected_ev_3 = make_touch(0, mir_touch_action_up);
    add_another_touch(expected_ev_3, 1, mir_touch_action_change);
    auto expected_ev_4 = make_touch(1, mir_touch_action_change);
    add_another_touch(expected_ev_4, 2, mir_touch_action_down);
    auto const& expected_ev_5 = touch_3;

    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_1)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_2)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_3)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_4)));
    EXPECT_CALL(next_dispatcher, dispatch(mt::MirTouchEventMatches(*expected_ev_5)));

    rewriter.dispatch(*touch_1);
    rewriter.dispatch(*touch_2);
    rewriter.dispatch(*touch_3);
}
