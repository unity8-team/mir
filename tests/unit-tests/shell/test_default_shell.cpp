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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/compositor/buffer_stream.h"
#include "mir/shell/session.h"
#include "src/server/surfaces/surface_impl.h"
#include "mir/shell/session_listener.h"
#include "mir/shell/null_session_listener.h"
#include "mir/shell/surface_creation_parameters.h"
#include "src/server/shell/default_shell.h"
#include "src/server/shell/session_event_sink.h"
#include "src/server/surfaces/basic_surface.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_surface_factory.h"
#include "mir_test_doubles/mock_focus_setter.h"
#include "mir_test_doubles/mock_session_listener.h"
#include "mir_test_doubles/stub_surface_builder.h"
#include "mir_test_doubles/stub_surface_controller.h"
#include "mir_test_doubles/null_snapshot_strategy.h"
#include "mir_test_doubles/null_surface_configurator.h"
#include "mir_test_doubles/null_event_sink.h"
#include "mir_test_doubles/null_session_event_sink.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{
struct MockSessionEventSink : public msh::SessionEventSink
{
    MOCK_METHOD1(handle_focus_change, void(std::shared_ptr<msh::Session> const& session));
    MOCK_METHOD0(handle_no_focus, void());
    MOCK_METHOD1(handle_session_stopping, void(std::shared_ptr<msh::Session> const& session));
};

struct DefaultShellSetup : public testing::Test
{
    DefaultShellSetup()
      : default_shell(mt::fake_shared(surface_factory),
                        mt::fake_shared(focus_setter),
                        std::make_shared<mtd::NullSnapshotStrategy>(),
                        std::make_shared<mtd::NullSessionEventSink>(),
                        mt::fake_shared(session_listener))
    {
        using namespace ::testing;
    }

    mtd::StubSurfaceBuilder surface_builder;
    mtd::StubSurfaceController surface_controller;
    mtd::MockSurfaceFactory surface_factory;
    testing::NiceMock<mtd::MockFocusSetter> focus_setter; // Inelegant but some tests need a stub
    msh::NullSessionListener session_listener;

    msh::DefaultShell default_shell;
};

}

TEST_F(DefaultShellSetup, open_and_close_session)
{
    using namespace ::testing;

    auto session = default_shell.open_session("Visual Basic Studio", std::shared_ptr<mf::EventSink>());
    default_shell.close_session(session);
}

TEST_F(DefaultShellSetup, closing_session_removes_surfaces)
{
    using namespace ::testing;

    EXPECT_CALL(surface_factory, create_surface(_, _, _, _)).Times(1);

    ON_CALL(surface_factory, create_surface(_, _, _, _)).WillByDefault(
       Return(std::make_shared<ms::SurfaceImpl>(
           nullptr,
           mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
           msh::a_surface(),mf::SurfaceId{}, std::shared_ptr<mf::EventSink>())));


    EXPECT_CALL(focus_setter, set_focus_to(_)).Times(1);
    EXPECT_CALL(focus_setter, set_focus_to(std::shared_ptr<msh::Session>())).Times(1);

    auto session = default_shell.open_session("Visual Basic Studio", std::shared_ptr<mf::EventSink>());
    session->create_surface(msh::a_surface().of_size(geom::Size{geom::Width{1024}, geom::Height{768}}));

    default_shell.close_session(session);
}

TEST_F(DefaultShellSetup, new_applications_receive_focus)
{
    using namespace ::testing;
    std::shared_ptr<msh::Session> new_session;

    {
        InSequence seq;
        EXPECT_CALL(focus_setter, set_focus_to(_)).WillOnce(SaveArg<0>(&new_session));
        EXPECT_CALL(focus_setter, set_focus_to(std::shared_ptr<msh::Session>()));
    }

    auto session = default_shell.open_session("Visual Basic Studio", std::shared_ptr<mf::EventSink>());
    EXPECT_EQ(session, new_session);
}

TEST_F(DefaultShellSetup, create_surface_for_session_forwards_and_then_focuses_session)
{
    using namespace ::testing;
    ON_CALL(surface_factory, create_surface(_, _, _, _)).WillByDefault(
        Return(std::make_shared<ms::SurfaceImpl>(
           nullptr,
           mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
           msh::a_surface(),mf::SurfaceId{}, std::shared_ptr<mf::EventSink>())));

    // Once for session creation and once for surface creation
    {
        InSequence seq;

        EXPECT_CALL(focus_setter, set_focus_to(_)).Times(1); // Session creation
        EXPECT_CALL(surface_factory, create_surface(_, _, _, _)).Times(1);
        EXPECT_CALL(focus_setter, set_focus_to(_)).Times(1); // Post Surface creation
        EXPECT_CALL(focus_setter, set_focus_to(std::shared_ptr<msh::Session>())); // Tear down
    }

    auto session1 = default_shell.open_session("Weather Report", std::shared_ptr<mf::EventSink>());
    default_shell.create_surface_for(session1, msh::a_surface());
}

TEST_F(DefaultShellSetup, cycle_focus)
{
    using namespace ::testing;

    EXPECT_CALL(focus_setter, set_focus_to(_)).Times(3);

    auto session1 = default_shell.open_session("Visual Basic Studio", std::make_shared<mtd::NullEventSink>());
    auto session2 = default_shell.open_session("Microsoft Access", std::make_shared<mtd::NullEventSink>());
    auto session3 = default_shell.open_session("WordPerfect", std::make_shared<mtd::NullEventSink>());

    Mock::VerifyAndClearExpectations(&focus_setter);

    {
      InSequence seq;
      EXPECT_CALL(focus_setter, set_focus_to(Eq(session1))).Times(1);
      EXPECT_CALL(focus_setter, set_focus_to(Eq(session2))).Times(1);
      EXPECT_CALL(focus_setter, set_focus_to(Eq(session3))).Times(1);
    }

    default_shell.focus_next();
    default_shell.focus_next();
    default_shell.focus_next();

    Mock::VerifyAndClearExpectations(&focus_setter);

    // Possible change of focus while sessions are closed on shutdown
    EXPECT_CALL(focus_setter, set_focus_to(_)).Times(AtLeast(0));
}

TEST_F(DefaultShellSetup, closing_applications_transfers_focus)
{
    using namespace ::testing;

    EXPECT_CALL(focus_setter, set_focus_to(_)).Times(3);

    auto session1 = default_shell.open_session("Visual Basic Studio", std::make_shared<mtd::NullEventSink>());
    auto session2 = default_shell.open_session("Microsoft Access", std::make_shared<mtd::NullEventSink>());
    auto session3 = default_shell.open_session("WordPerfect", std::make_shared<mtd::NullEventSink>());

    Mock::VerifyAndClearExpectations(&focus_setter);

    {
      InSequence seq;
      EXPECT_CALL(focus_setter, set_focus_to(Eq(session2))).Times(1);
      EXPECT_CALL(focus_setter, set_focus_to(Eq(session1))).Times(1);
    }

    default_shell.close_session(session3);
    default_shell.close_session(session2);

    Mock::VerifyAndClearExpectations(&focus_setter);

    // Possible change of focus while sessions are closed on shutdown
    EXPECT_CALL(focus_setter, set_focus_to(_)).Times(AtLeast(0));
}

TEST_F(DefaultShellSetup, for_each)
{
    using namespace ::testing;

    auto session1 = default_shell.open_session("Roll Away", std::shared_ptr<mf::EventSink>());
    auto session2 = default_shell.open_session("The", std::shared_ptr<mf::EventSink>());
    auto session3 = default_shell.open_session("Dew", std::shared_ptr<mf::EventSink>());

    struct
    {
        MOCK_METHOD1(see, void(std::shared_ptr<mf::Session> const&));
        void operator()(std::shared_ptr<mf::Session> const& session)
        {
            see(session);
        }
    } observer;

    InSequence seq;
    EXPECT_CALL(observer, see(session1));
    EXPECT_CALL(observer, see(session2));
    EXPECT_CALL(observer, see(session3));
    default_shell.for_each(std::ref(observer));
}


namespace 
{

struct DefaultShellSessionListenerSetup : public testing::Test
{
    DefaultShellSessionListenerSetup()
      : default_shell(mt::fake_shared(surface_factory),
                        mt::fake_shared(focus_setter),
                        std::make_shared<mtd::NullSnapshotStrategy>(),
                        std::make_shared<mtd::NullSessionEventSink>(),
                        mt::fake_shared(session_listener))
    {
        using namespace ::testing;
    }

    mtd::MockSurfaceFactory surface_factory;
    testing::NiceMock<mtd::MockFocusSetter> focus_setter; // Inelegant but some tests need a stub
    mtd::MockSessionListener session_listener;

    msh::DefaultShell default_shell;
};
}

TEST_F(DefaultShellSessionListenerSetup, session_listener_is_notified_of_lifecycle_and_focus)
{
    using namespace ::testing;

    EXPECT_CALL(session_listener, starting(_)).Times(1);
    EXPECT_CALL(session_listener, focused(_)).Times(1);
    EXPECT_CALL(session_listener, stopping(_)).Times(1);
    EXPECT_CALL(session_listener, unfocused()).Times(1);

    auto session = default_shell.open_session("XPlane", std::shared_ptr<mf::EventSink>());
    default_shell.close_session(session);
}

namespace
{

struct DefaultShellSessionEventsSetup : public testing::Test
{
    DefaultShellSessionEventsSetup()
      : default_shell(mt::fake_shared(surface_factory),
                        mt::fake_shared(focus_setter),
                        std::make_shared<mtd::NullSnapshotStrategy>(),
                        mt::fake_shared(session_event_sink),
                        std::make_shared<msh::NullSessionListener>())
    {
        using namespace ::testing;
    }

    mtd::MockSurfaceFactory surface_factory;
    testing::NiceMock<mtd::MockFocusSetter> focus_setter; // Inelegant but some tests need a stub
    MockSessionEventSink session_event_sink;

    msh::DefaultShell default_shell;
};

}

TEST_F(DefaultShellSessionEventsSetup, session_event_sink_is_notified_of_lifecycle_and_focus)
{
    using namespace ::testing;

    EXPECT_CALL(session_event_sink, handle_focus_change(_)).Times(2);

    auto session = default_shell.open_session("XPlane", std::shared_ptr<mf::EventSink>());
    auto session1 = default_shell.open_session("Bla", std::shared_ptr<mf::EventSink>());

    Mock::VerifyAndClearExpectations(&session_event_sink);

    InSequence s;
    EXPECT_CALL(session_event_sink, handle_session_stopping(_)).Times(1);
    EXPECT_CALL(session_event_sink, handle_focus_change(_)).Times(1);
    EXPECT_CALL(session_event_sink, handle_session_stopping(_)).Times(1);
    EXPECT_CALL(session_event_sink, handle_no_focus()).Times(1);

    default_shell.close_session(session1);
    default_shell.close_session(session);
}
