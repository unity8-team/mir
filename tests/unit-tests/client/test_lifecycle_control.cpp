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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "src/client/lifecycle_control.h"

#include <gtest/gtest.h>

namespace
{
bool a_called{false}, b_called{false};

void handler_a(MirLifecycleState)
{
    a_called = true;
}

void handler_b(MirLifecycleState)
{
    b_called = true;
}
}

TEST(LifeCycleControl, InvokesSetTarget)
{
    mir::client::LifecycleControl control;

    a_called = false;
    b_called = false;

    control.set_lifecycle_event_handler(&handler_a);
    control.call_lifecycle_event_handler(mir_lifecycle_connection_lost);

    EXPECT_TRUE(a_called);
    EXPECT_FALSE(b_called);
}

TEST(LifeCycleControl, InvokesUpdatedTarget)
{
    mir::client::LifecycleControl control;

    a_called = false;
    b_called = false;

    control.set_lifecycle_event_handler(&handler_a);
    control.set_lifecycle_event_handler(&handler_b);
    control.call_lifecycle_event_handler(mir_lifecycle_state_resumed);

    EXPECT_FALSE(a_called);
    EXPECT_TRUE(b_called);
}

TEST(LifeCycleControl, UpdateReplacesIfMatches)
{
    mir::client::LifecycleControl control;

    a_called = false;
    b_called = false;

    control.set_lifecycle_event_handler(&handler_a);
    control.replace_lifecycle_event_handler_if_matches(&handler_a, &handler_b);
    control.call_lifecycle_event_handler(mir_lifecycle_state_resumed);

    EXPECT_FALSE(a_called);
    EXPECT_TRUE(b_called);
}

TEST(LifeCycleControl, UpdateDoesNotReplaceIfNotMatching)
{
    mir::client::LifecycleControl control;

    a_called = false;
    b_called = false;

    control.set_lifecycle_event_handler(&handler_a);
    control.replace_lifecycle_event_handler_if_matches(&handler_b, &handler_b);
    control.call_lifecycle_event_handler(mir_lifecycle_state_resumed);

    EXPECT_TRUE(a_called);
    EXPECT_FALSE(b_called);
}

TEST(LifeCycleControl, UpdateHandlesDifferingTypes)
{
    mir::client::LifecycleControl control;

    a_called = false;
    b_called = false;
    auto non_fn_pointer_handler = [](MirLifecycleState){};

    control.set_lifecycle_event_handler(non_fn_pointer_handler);
    control.replace_lifecycle_event_handler_if_matches(&handler_a, &handler_b);
    control.call_lifecycle_event_handler(mir_lifecycle_state_resumed);

    EXPECT_FALSE(a_called);
    EXPECT_FALSE(b_called);
}
