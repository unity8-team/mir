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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "src/server/scene/managed_surface.h"
#include "mir_test_doubles/mock_surface.h"
#include "mir_test_doubles/mock_display_layout.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;
using namespace mir::scene;
using namespace mir::geometry;
using namespace mir::test::doubles;

struct ManagedSurfaceTest : public Test
{
    std::shared_ptr<MockDisplayLayout>
        mock_display_layout{std::make_shared<NiceMock<MockDisplayLayout>>()};

    std::shared_ptr<MockSurface>
        mock_basic_surface{std::make_shared<NiceMock<MockSurface>>()};
    MirSurfaceState state_ = mir_surface_state_restored;
    Rectangle position{{12,34}, {56,78}};
    Rectangle const fullscreen{{0,0}, {1366,768}};

    void SetUp()
    {
        ON_CALL(*mock_display_layout, size_to_output(_))
            .WillByDefault(SetArgReferee<0>(fullscreen));

        // This could be served better by a FakeSurface later, but this
        // requires less code for now...
        ON_CALL(*mock_basic_surface, size())
            .WillByDefault(ReturnPointee(&position.size));
        ON_CALL(*mock_basic_surface, resize(_))
            .WillByDefault(SaveArg<0>(&position.size));

        ON_CALL(*mock_basic_surface, top_left())
            .WillByDefault(ReturnPointee(&position.top_left));
        ON_CALL(*mock_basic_surface, move_to(_))
            .WillByDefault(SaveArg<0>(&position.top_left));

        ON_CALL(*mock_basic_surface, state())
            .WillByDefault(ReturnPointee(&state_));
        ON_CALL(*mock_basic_surface, configure(mir_surface_attrib_state, _))
            .WillByDefault(SaveArg<1>((int*)&state_));
    }
};

TEST_F(ManagedSurfaceTest, goes_fullscreen)
{
    ManagedSurface surf(mock_basic_surface, mock_display_layout);

    Rectangle const restored{surf.top_left(), surf.size()};
    ASSERT_NE(fullscreen, restored);

    InSequence seq;
    EXPECT_CALL(*mock_basic_surface, move_to(fullscreen.top_left));
    EXPECT_CALL(*mock_basic_surface, resize(fullscreen.size));
    EXPECT_CALL(*mock_basic_surface, move_to(restored.top_left));
    EXPECT_CALL(*mock_basic_surface, resize(restored.size));

    surf.configure(mir_surface_attrib_state, mir_surface_state_fullscreen);
    surf.configure(mir_surface_attrib_state, mir_surface_state_restored);
}

