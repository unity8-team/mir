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
    Rectangle const maximized{fullscreen}; // will be different in future

    void SetUp()
    {
        ON_CALL(*mock_display_layout, size_to_output(_))
            .WillByDefault(SetArgReferee<0>(fullscreen));

        // This could be served better by a FakeSurface later, but this
        // requires less code for now.
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

TEST_F(ManagedSurfaceTest, goes_fullscreen_and_restores)
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

TEST_F(ManagedSurfaceTest, goes_maximized_and_restores)
{
    ManagedSurface surf(mock_basic_surface, mock_display_layout);

    Rectangle const restored{surf.top_left(), surf.size()};
    ASSERT_NE(maximized, restored);

    InSequence seq;
    EXPECT_CALL(*mock_basic_surface, move_to(maximized.top_left));
    EXPECT_CALL(*mock_basic_surface, resize(maximized.size));
    EXPECT_CALL(*mock_basic_surface, move_to(restored.top_left));
    EXPECT_CALL(*mock_basic_surface, resize(restored.size));

    surf.configure(mir_surface_attrib_state, mir_surface_state_maximized);
    surf.configure(mir_surface_attrib_state, mir_surface_state_restored);
}

TEST_F(ManagedSurfaceTest, goes_vertmaximized_and_restores)
{
    ManagedSurface surf(mock_basic_surface, mock_display_layout);

    Rectangle const restored{surf.top_left(), surf.size()};
    ASSERT_NE(maximized, restored);

    Rectangle const vertmaximized{
        {restored.top_left.x, maximized.top_left.y},
        {restored.size.width, maximized.size.height} };

    InSequence seq;
    EXPECT_CALL(*mock_basic_surface, move_to(vertmaximized.top_left));
    EXPECT_CALL(*mock_basic_surface, resize(vertmaximized.size));
    EXPECT_CALL(*mock_basic_surface, move_to(restored.top_left));
    EXPECT_CALL(*mock_basic_surface, resize(restored.size));

    surf.configure(mir_surface_attrib_state, mir_surface_state_vertmaximized);
    surf.configure(mir_surface_attrib_state, mir_surface_state_restored);
}

TEST_F(ManagedSurfaceTest, goes_minimized_and_restores)
{
    ManagedSurface surf(mock_basic_surface, mock_display_layout);

    InSequence seq;
    EXPECT_CALL(*mock_basic_surface, hide());
    EXPECT_CALL(*mock_basic_surface, show());

    surf.configure(mir_surface_attrib_state, mir_surface_state_minimized);
    surf.configure(mir_surface_attrib_state, mir_surface_state_restored);
}

TEST_F(ManagedSurfaceTest, multistate_restores_to_original)
{
    ManagedSurface surf(mock_basic_surface, mock_display_layout);
    Rectangle const restored{surf.top_left(), surf.size()};

    surf.configure(mir_surface_attrib_state, mir_surface_state_vertmaximized);
    surf.move_to({surf.top_left().x.as_int() + 5, surf.top_left().y});
    EXPECT_NE(restored.top_left, surf.top_left());

    surf.configure(mir_surface_attrib_state, mir_surface_state_fullscreen);
    EXPECT_EQ(fullscreen.top_left, surf.top_left());
    EXPECT_EQ(fullscreen.size, surf.size());

    surf.configure(mir_surface_attrib_state, mir_surface_state_restored);
    EXPECT_EQ(restored.top_left, surf.top_left());
    EXPECT_EQ(restored.size, surf.size());
}


