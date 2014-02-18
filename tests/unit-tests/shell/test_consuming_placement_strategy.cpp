/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir_test_doubles/mock_display_layout.h"
#include "mir_test_doubles/stub_shell_session.h"
#include "mir_test_doubles/mock_surface.h"

#include "src/server/shell/consuming_placement_strategy.h"
#include "mir/shell/surface_creation_parameters.h"

#include "mir/geometry/rectangle.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#if 0 // TODO
namespace msh = mir::shell;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;

namespace
{

struct ConsumingPlacementStrategySetup : public testing::Test
{
    void SetUp()
    {
        using namespace ::testing;
        display_layout = std::make_shared<mtd::MockDisplayLayout>();
    }

    std::shared_ptr<mtd::MockDisplayLayout> display_layout;
    mtd::StubShellSession session;
};
}


TEST_F(ConsumingPlacementStrategySetup, parameters_with_no_geometry_are_made_fullscreen)
{
    using namespace ::testing;

    geom::Rectangle rect{{1, 2}, {0, 0}};
    NiceMock<mtd::MockSurface> mock_surface;  // TODO: Stub(shell)Surface

    EXPECT_CALL(*display_layout, size_to_output(rect)).Times(1);

    msh::ConsumingPlacementStrategy placement_strategy(display_layout);

    placement_strategy.place(mock_surface);
}

TEST_F(ConsumingPlacementStrategySetup, parameters_with_geometry_are_clipped)
{
    using namespace ::testing;

    geom::Rectangle rect{{0, 0}, {100, 200}};
    NiceMock<mtd::MockSurface> mock_surface;  // TODO: Stub(shell)Surface

    EXPECT_CALL(*display_layout, clip_to_output(rect)).Times(1);

    msh::ConsumingPlacementStrategy placement_strategy(display_layout);

    placement_strategy.place(mock_surface);
}

TEST_F(ConsumingPlacementStrategySetup, parameters_with_output_id_are_placed_in_output)
{
    using namespace ::testing;

    msh::SurfaceCreationParameters input_params;
    input_params.size = geom::Size{100, 200};
    input_params.output_id = mir::graphics::DisplayConfigurationOutputId{1};
    geom::Rectangle rect{input_params.top_left, input_params.size};

    EXPECT_CALL(*display_layout,
                place_in_output(input_params.output_id, rect))
        .Times(1);

    msh::ConsumingPlacementStrategy placement_strategy(display_layout);

    placement_strategy.place(session, input_params);
}
#endif
