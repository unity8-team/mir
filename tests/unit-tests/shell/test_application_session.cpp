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
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#include "mir/shell/application_session.h"
#include "mir/compositor/buffer.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_surface_factory.h"
#include "mir_test_doubles/mock_surface.h"
#include "mir_test_doubles/stub_surface.h"

#include "mir/shell/surface.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace me = mir::events;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace mi = mir::input;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

TEST(ApplicationSession, adopt_and_abandon_surface)
{
    msh::ApplicationSession session("Foo");

    auto surface1 = std::make_shared<mtd::StubSurface>();
    auto surface2 = std::make_shared<mtd::StubSurface>();

    auto id1 = session.adopt_surface(surface1);
    auto id2 = session.adopt_surface(surface1);

    EXPECT_EQ(surface2, session.abandon_surface(id2);
    EXPECT_EQ(surface1, session.abandon_surface(id1);
}

TEST(ApplicationSession, default_surface_is_first_surface)
{
    msh::ApplicationSession app_session("Foo");

    auto surface1 = std::make_shared<mtd::StubSurface>();
    auto surface2 = std::make_shared<mtd::StubSurface>();
    auto surface3 = std::make_shared<mtd::StubSurface>();
    auto id1 = app_session.adopt_surface(surface1);
    auto id2 = app_session.adopt_surface(surface2);
    auto id3 = app_session.adopt_surface(surface3);
    EXPECT_EQ(app_session.get_surface(id1), surface1);
    EXPECT_EQ(app_session.get_surface(id2), surface2);
    EXPECT_EQ(app_session.get_surface(id3), surface3);

    auto default_surf = app_session.default_surface();
    EXPECT_EQ(surface1, default_surf);
    app_session.abandon_surface(id1);

    default_surf = app_session.default_surface();
    EXPECT_EQ(surface2, default_surf);
    app_session.abandon_surface(id2);

    default_surf = app_session.default_surface();
    EXPECT_EQ(surface3, default_surf);
    app_session.abandon_surface(id3);
}

#if 0
TEST(ApplicationSession, session_visbility_propagates_to_surfaces)
{
    using namespace ::testing;

    mtd::StubSurfaceBuilder surface_builder;
    auto const mock_surface = std::make_shared<mtd::MockSurface>(mt::fake_shared(surface_builder));

    mtd::MockSurfaceFactory surface_factory;
    ON_CALL(surface_factory, create_surface(_, _, _)).WillByDefault(Return(mock_surface));

    msh::ApplicationSession app_session(mt::fake_shared(surface_factory), "Foo");

    EXPECT_CALL(surface_factory, create_surface(_, _, _));

    {
        InSequence seq;
        EXPECT_CALL(*mock_surface, hide()).Times(1);
        EXPECT_CALL(*mock_surface, show()).Times(1);
        EXPECT_CALL(*mock_surface, destroy()).Times(1);
    }

    msh::SurfaceCreationParameters params;
    auto surf = app_session.create_surface(params);

    app_session.hide();
    app_session.show();

    app_session.destroy_surface(surf);
}
#endif

TEST(Session, get_invalid_surface_throw_behavior)
{
    using namespace ::testing;

    msh::ApplicationSession app_session("Foo");
    mf::SurfaceId invalid_surface_id(1);

    EXPECT_THROW({
            app_session.get_surface(invalid_surface_id);
    }, std::runtime_error);
}
