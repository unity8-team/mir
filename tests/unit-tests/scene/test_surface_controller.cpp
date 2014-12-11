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
    mtd::MockSurface mock_surface;
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
}

TEST_F(SurfaceController, add_wrap_and_remove_surface)
{
    using namespace ::testing;

    class MockSurfaceController : public ms::SurfaceController
    {
    public:
        MockSurfaceController(
            std::shared_ptr<ms::SurfaceFactory> const& surface_factory,
            std::shared_ptr<ms::PlacementStrategy> const& placement_strategy,
            std::shared_ptr<msh::DisplayLayout> const& display_layout,
            std::shared_ptr<ms::SurfaceStackModel> const& surface_stack)
            : ms::SurfaceController(surface_factory,
                                    placement_strategy,
                                    display_layout,
                                    surface_stack)
        {
        }

        MOCK_METHOD1(wrap_surface, std::shared_ptr<ms::Surface>(
                                     std::shared_ptr<ms::Surface> const&));
    };

    MockSurfaceController controller(
        mt::fake_shared(mock_surface_allocator),
        mt::fake_shared(placement_strategy),
        mt::fake_shared(mock_display_layout),
        mt::fake_shared(model));

    EXPECT_CALL(controller, wrap_surface(expect_surface))
        .WillOnce(Return(expect_surface));

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
