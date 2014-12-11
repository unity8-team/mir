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
        mock_display_layout{std::make_shared<MockDisplayLayout>()};

    std::shared_ptr<NiceMock<MockSurface>>
        mock_basic_surface{std::make_shared<NiceMock<MockSurface>>()};
    Rectangle position;

    Rectangle const fullscreen{{0,0}, {1366,768}};

    void SetUp()
    {
        ON_CALL(*mock_display_layout, size_to_output(_))
            .WillByDefault(SetArgReferee<0>(fullscreen));

        ON_CALL(*mock_basic_surface, size())
            .WillByDefault(Return(position.size));
        ON_CALL(*mock_basic_surface, resize(_))
            .WillByDefault(SaveArg<0>(&position.size));
        ON_CALL(*mock_basic_surface, top_left())
            .WillByDefault(Return(position.top_left));
        ON_CALL(*mock_basic_surface, move_to(_))
            .WillByDefault(SaveArg<0>(&position.top_left));
    }
};

TEST_F(ManagedSurfaceTest, foo)
{
    ManagedSurface surf(mock_basic_surface, mock_display_layout);
}

