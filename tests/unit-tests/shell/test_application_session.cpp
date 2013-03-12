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
#include "mir/shell/focus_arbitrator.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_surface_factory.h"
#include "mir_test_doubles/mock_surface.h"

#include "src/surfaces/proxy_surface.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace mi = mir::input;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{

struct MockFocusArbitrator : public msh::FocusArbitrator
{
    MOCK_METHOD1(request_focus, bool(std::shared_ptr<msh::Session> const&));
};

}

TEST(ApplicationSession, create_and_destroy_surface)
{
    using namespace ::testing;

    auto const mock_surface = std::make_shared<mtd::MockSurface>();
    mtd::MockSurfaceFactory surface_factory;
    NiceMock<MockFocusArbitrator> focus_arbitrator;

    ON_CALL(surface_factory, create_surface(_)).WillByDefault(Return(mock_surface));

    EXPECT_CALL(surface_factory, create_surface(_));
    EXPECT_CALL(*mock_surface, destroy());

    auto session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");

    msh::SurfaceCreationParameters params;
    auto surf = session->create_surface(params);

    session->destroy_surface(surf);
}

TEST(ApplicationSession, must_be_managed_by_shared_ptr_to_create_surface)
{
    using namespace ::testing;

    auto const mock_surface = std::make_shared<NiceMock<mtd::MockSurface>>();
    NiceMock<mtd::MockSurfaceFactory> surface_factory;
    NiceMock<MockFocusArbitrator> focus_arbitrator;

    ON_CALL(surface_factory, create_surface(_)).WillByDefault(Return(mock_surface));

    auto session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");
    msh::ApplicationSession unmanaged_session(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");
    
    EXPECT_NO_THROW({
            session->create_surface(msh::a_surface());
     });

    // Since create surface uses std::enable_shared_from this to request focus from the focus arbitrator 
    // creating a surface from an unmanaged session will throw
    EXPECT_THROW({
            unmanaged_session.create_surface(msh::a_surface());
    }, std::bad_weak_ptr);
}


TEST(ApplicationSession, session_visbility_propagates_to_surfaces)
{
    using namespace ::testing;

    auto const mock_surface = std::make_shared<mtd::MockSurface>();
    mtd::MockSurfaceFactory surface_factory;
    NiceMock<MockFocusArbitrator> focus_arbitrator;

    ON_CALL(surface_factory, create_surface(_)).WillByDefault(Return(mock_surface));

    auto app_session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");

    EXPECT_CALL(surface_factory, create_surface(_));

    {
        InSequence seq;
        EXPECT_CALL(*mock_surface, hide()).Times(1);
        EXPECT_CALL(*mock_surface, show()).Times(1);
        EXPECT_CALL(*mock_surface, destroy()).Times(1);
    }

    msh::SurfaceCreationParameters params;
    auto surf = app_session->create_surface(params);

    app_session->hide();
    app_session->show();

    app_session->destroy_surface(surf);
}

TEST(ApplicationSession, creating_suface_requests_focus)
{
    using namespace ::testing;

    mtd::MockSurfaceFactory surface_factory;
    mtd::MockSurface mock_surface;
    MockFocusArbitrator focus_arbitrator;

    ON_CALL(surface_factory, create_surface(_)).WillByDefault(Return(mt::fake_shared(mock_surface)));
    EXPECT_CALL(focus_arbitrator, request_focus(_)).Times(1).WillOnce(Return(true));

    auto app_session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");
    app_session->create_surface(msh::a_surface());
}

TEST(ApplicationSession, session_has_appeared_after_creating_surface)
{
    using namespace ::testing;

    mtd::MockSurfaceFactory surface_factory;
    mtd::MockSurface mock_surface;
    NiceMock<MockFocusArbitrator> focus_arbitrator;

    ON_CALL(surface_factory, create_surface(_)).WillByDefault(Return(mt::fake_shared(mock_surface)));

    auto app_session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");
    EXPECT_FALSE(app_session->has_appeared());
    app_session->create_surface(msh::a_surface());
    EXPECT_TRUE(app_session->has_appeared());
}

TEST(ApplicationSession, get_invalid_surface_throw_behavior)
{
    using namespace ::testing;

    mtd::MockSurfaceFactory surface_factory;
    NiceMock<MockFocusArbitrator> focus_arbitrator;
    auto app_session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");
    msh::SurfaceId invalid_surface_id = msh::SurfaceId{1};

    EXPECT_THROW({
            app_session->get_surface(invalid_surface_id);
    }, std::runtime_error);
}

TEST(ApplicationSession, destroy_invalid_surface_throw_behavior)
{
    using namespace ::testing;

    mtd::MockSurfaceFactory surface_factory;
    NiceMock<MockFocusArbitrator> focus_arbitrator;
    auto app_session = std::make_shared<msh::ApplicationSession>(mt::fake_shared(surface_factory), focus_arbitrator, "Foo");
    msh::SurfaceId invalid_surface_id = msh::SurfaceId{1};

    EXPECT_THROW({
            app_session->destroy_surface(invalid_surface_id);
    }, std::runtime_error);
}
