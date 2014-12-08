/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#include "src/server/scene/surface_controller.h"
#include "src/server/scene/surface_stack_model.h"
#include "mir/scene/surface_factory.h"
#include "mir/scene/placement_strategy.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir_test_doubles/stub_scene_session.h"

#include "mir_test_doubles/mock_surface.h"
#include "mir_test_doubles/mock_display_layout.h"
#include "mir_test_doubles/stub_scene_surface.h"
#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace geom = mir::geometry;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace ms = mir::scene;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{
struct MockSurfaceAllocator : public ms::SurfaceFactory
{
    MOCK_METHOD1(create_surface, std::shared_ptr<ms::Surface>(
        ms::SurfaceCreationParameters const&));
};

struct MockPlacementStrategy : public ms::PlacementStrategy
{
    MOCK_METHOD2(place, ms::SurfaceCreationParameters(ms::Session const&, ms::SurfaceCreationParameters const&));
};

struct MockSurfaceStackModel : public ms::SurfaceStackModel
{
    MOCK_METHOD3(add_surface, void(
        std::shared_ptr<ms::Surface> const&,
        ms::DepthId depth,
        mir::input::InputReceptionMode input_mode));
    MOCK_METHOD1(remove_surface, void(std::weak_ptr<ms::Surface> const&));
    MOCK_METHOD1(raise, void(std::weak_ptr<ms::Surface> const&));
};

struct SurfaceController : testing::Test
{
    MockPlacementStrategy placement_strategy;
    testing::NiceMock<mtd::MockSurface> mock_surface;
    std::shared_ptr<ms::Surface> const expect_surface = mt::fake_shared(mock_surface);
    testing::NiceMock<MockSurfaceAllocator> mock_surface_allocator;
    testing::NiceMock<mtd::MockDisplayLayout> mock_display_layout;
    MockSurfaceStackModel model;
    mtd::StubSceneSession session;

    void SetUp()
    {
        using namespace ::testing;
        ON_CALL(mock_surface_allocator, create_surface(_)).WillByDefault(Return(expect_surface));
        ON_CALL(placement_strategy, place(_, _)).WillByDefault(ReturnArg<1>());
    }
};

geom::Point dragged_cursor(ms::Surface const& s,
                           geom::Displacement const& grab,
                           geom::Displacement const& delta)
{
    return s.top_left() + delta + grab;
}

} // namespace

TEST_F(SurfaceController, add_and_remove_surface)
{
    using namespace ::testing;

    ms::SurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    InSequence seq;
    EXPECT_CALL(placement_strategy, place(_, _)).Times(1);
    EXPECT_CALL(mock_surface_allocator, create_surface(_)).Times(1).WillOnce(Return(expect_surface));
    EXPECT_CALL(model, add_surface(_,_,_)).Times(1);
    EXPECT_CALL(model, remove_surface(_)).Times(1);

    auto actual_surface = controller.add_surface(ms::a_surface(), &session);

    EXPECT_THAT(actual_surface, Eq(expect_surface));
    controller.remove_surface(actual_surface);
}

TEST_F(SurfaceController, raise_surface)
{
    using namespace ::testing;

    ms::SurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    EXPECT_CALL(model, raise(_)).Times(1);

    controller.raise(std::weak_ptr<ms::Surface>());
}

TEST_F(SurfaceController, offers_create_surface_parameters_to_placement_strategy)
{
    using namespace ::testing;
    EXPECT_CALL(mock_surface, add_observer(_)).Times(AnyNumber());
    EXPECT_CALL(model, add_surface(_,_,_)).Times(AnyNumber());

    ms::SurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    auto params = ms::a_surface();
    EXPECT_CALL(placement_strategy, place(Ref(session), Ref(params))).Times(1)
        .WillOnce(Return(ms::a_surface()));

    controller.add_surface(params, &session);
}

TEST_F(SurfaceController, forwards_create_surface_parameters_from_placement_strategy_to_underlying_factory)
{
    using namespace ::testing;
    EXPECT_CALL(mock_surface, add_observer(_)).Times(AnyNumber());
    EXPECT_CALL(model, add_surface(_,_,_)).Times(AnyNumber());

    ms::SurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    auto params = ms::a_surface();
    auto placed_params = params;
    placed_params.size.width = geom::Width{100};

    EXPECT_CALL(placement_strategy, place(_, Ref(params))).Times(1)
        .WillOnce(Return(placed_params));
    EXPECT_CALL(mock_surface_allocator, create_surface(placed_params));

    controller.add_surface(params, &session);
}

TEST_F(SurfaceController, sets_states)
{
    using namespace ::testing;

    geom::Rectangle const restored{{12,34}, {67,89}},
                          fullscreen{{0,0}, {1366,768}},
                          maximized(fullscreen),  // TODO will be different
                          vertmaximized{{restored.top_left.x,
                                         maximized.top_left.y},
                                        {restored.size.width,
                                         maximized.size.height}};

    ms::SurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    EXPECT_CALL(mock_display_layout, size_to_output(_))
        .WillRepeatedly(SetArgReferee<0>(fullscreen));

    mtd::StubSceneSurface surface(restored);
    ASSERT_EQ(restored.top_left, surface.top_left());
    ASSERT_EQ(restored.size, surface.size());

    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_vertmaximized);
    EXPECT_EQ(vertmaximized.size, surface.size());
    EXPECT_EQ(vertmaximized.top_left, surface.top_left());

    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_maximized);
    EXPECT_EQ(maximized.size, surface.size());
    EXPECT_EQ(maximized.top_left, surface.top_left());

    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_fullscreen);
    EXPECT_EQ(fullscreen.size, surface.size());
    EXPECT_EQ(fullscreen.top_left, surface.top_left());

    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_restored);
#if 0
    // This does nothing. Restoration is implemented in BasicSurface, which
    // while convenient is not completely consistent. See BasicSurfaceTest
    // for tests of mir_surface_state_restored.
    EXPECT_EQ(restored.size, surface.size());
    EXPECT_EQ(restored.top_left, surface.top_left());
#endif
}

TEST_F(SurfaceController, drag_surface)
{
    using namespace ::testing;

    geom::Rectangle const restored{{12,34}, {67,89}};
    geom::Displacement const original_grab{33,5},
                             short_drag{5,-8},
                             long_y_drag{-14,+62},
                             long_drag{-54,+32};

    ms::SurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    mtd::StubSceneSurface surface(restored);

    // Drag while "restored"
    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_restored);
    auto grab = original_grab;
    auto prev_pos = surface.top_left();
    auto prev_size = surface.size();
    controller.drag_surface(surface, grab,
                            dragged_cursor(surface, grab, short_drag));
    // Short drag of a restored surface: always works
    EXPECT_EQ(prev_pos+short_drag, surface.top_left());
    EXPECT_EQ(prev_size, surface.size());
    grab = original_grab;
    prev_pos = surface.top_left();
    prev_size = surface.size();
    controller.drag_surface(surface, grab,
                            dragged_cursor(surface, grab, long_drag));
    // Long drag of a restored surface: always works
    EXPECT_EQ(prev_pos+long_drag, surface.top_left());
    EXPECT_EQ(prev_size, surface.size());

    // Drag while maximized
    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_maximized);
    prev_pos = surface.top_left();
    prev_size = surface.size();
    controller.drag_surface(surface, grab,
                            dragged_cursor(surface, grab, short_drag));
    // Short drag of a maximized surface: Does nothing
    EXPECT_EQ(prev_pos, surface.top_left());
    EXPECT_EQ(prev_size, surface.size());
    prev_pos = surface.top_left();
    prev_size = surface.size();
    controller.drag_surface(surface, grab,
                            dragged_cursor(surface, grab, long_drag));
    // Long drag of a maximized surface: Unsnaps and moves it
    EXPECT_EQ(prev_pos+long_drag, surface.top_left());
    EXPECT_EQ(prev_size, surface.size());

    // Drag while fullscreen: TODO figure out preferred behaviour. At the
    // moment it's just identical to maximized.

    // Drag while vertmaximized
    controller.configure_surface(surface, mir_surface_attrib_state,
                                 mir_surface_state_vertmaximized);
    prev_pos = surface.top_left();
    prev_size = surface.size();
    controller.drag_surface(surface, grab,
                            dragged_cursor(surface, grab, short_drag));
    // Short drag of a vertmaximized surface: Moves X but not Y
    geom::Point const short_drag_vertmaximized{
        prev_pos.x.as_int()+short_drag.dx.as_int(), prev_pos.y};
    EXPECT_EQ(short_drag_vertmaximized, surface.top_left());
    EXPECT_EQ(prev_size, surface.size());
    prev_pos = surface.top_left();
    prev_size = surface.size();
    controller.drag_surface(surface, grab,
                            dragged_cursor(surface, grab, long_y_drag));
    // Long drag of a vertmaximized surface: Unsnaps and moves it; X and Y
    EXPECT_EQ(prev_pos+long_y_drag, surface.top_left());
    EXPECT_EQ(prev_size, surface.size());
}
