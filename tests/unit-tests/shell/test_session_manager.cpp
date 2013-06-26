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

#include "mir/surfaces/buffer_stream.h"
#include "mir/surfaces/surface_stack_model.h"
#include "mir/shell/focus_sequence.h"
#include "mir/shell/session_manager.h"
#include "mir/shell/default_session_container.h"
#include "mir/shell/session.h"
#include "mir/shell/surface.h"
#include "mir/shell/session_listener.h"
#include "mir/shell/null_session_listener.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_surface_factory.h"
#include "mir_test_doubles/mock_focus_setter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace me = mir::events;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{
struct MockSurfaceStackModel : public ms::SurfaceStackModel
{
    MOCK_METHOD2(create_surface, std::weak_ptr<ms::Surface>(msh::SurfaceCreationParameters const&, ms::DepthId));
    MOCK_METHOD1(destroy_surface, void(std::weak_ptr<ms::Surface> const& surface));
};

struct MockSessionContainer : public msh::DefaultSessionContainer
{
    MOCK_METHOD1(insert_session, void(std::shared_ptr<msh::Session> const&));
    MOCK_METHOD1(remove_session, void(std::shared_ptr<msh::Session> const&));
    MOCK_METHOD0(lock, void());
    MOCK_METHOD0(unlock, void());
    ~MockSessionContainer() noexcept {}
};

struct MockFocusSequence : public msh::FocusSequence
{
    MOCK_CONST_METHOD1(successor_of, std::shared_ptr<msh::Session>(std::shared_ptr<msh::Session> const&));
    MOCK_CONST_METHOD1(predecessor_of, std::shared_ptr<msh::Session>(std::shared_ptr<msh::Session> const&));
    MOCK_CONST_METHOD0(default_focus, std::shared_ptr<msh::Session>());
};

struct SessionManagerSetup : public testing::Test
{
    SessionManagerSetup()
      : session_manager(mt::fake_shared(surface_stack),
                        mt::fake_shared(surface_factory),
                        mt::fake_shared(container),
                        mt::fake_shared(focus_sequence),
                        mt::fake_shared(focus_setter),
                        mt::fake_shared(session_listener))
    {
    }

    MockSurfaceStackModel surface_stack;
    mtd::MockSurfaceFactory surface_factory;
    testing::NiceMock<MockSessionContainer> container;    // Inelegant but some tests need a stub
    MockFocusSequence focus_sequence;
    testing::NiceMock<mtd::MockFocusSetter> focus_setter; // Inelegant but some tests need a stub
    msh::NullSessionListener session_listener;

    msh::SessionManager session_manager;
};

}

TEST_F(SessionManagerSetup, open_and_close_session)
{
    using namespace ::testing;

    EXPECT_CALL(container, insert_session(_)).Times(1);
    EXPECT_CALL(container, remove_session(_)).Times(1);
    EXPECT_CALL(focus_setter, set_focus_to(_));
    EXPECT_CALL(focus_setter, set_focus_to(std::shared_ptr<msh::Session>())).Times(1);
    EXPECT_CALL(focus_sequence, default_focus()).WillOnce(Return((std::shared_ptr<msh::Session>())));

    auto session = session_manager.open_session("Visual Basic Studio", std::shared_ptr<me::EventSink>());
    session_manager.close_session(session);
}

TEST_F(SessionManagerSetup, closing_session_removes_surfaces)
{
    using namespace ::testing;

    EXPECT_CALL(container, insert_session(_)).Times(1);
    EXPECT_CALL(container, remove_session(_)).Times(1);

    EXPECT_CALL(focus_setter, set_focus_to(_)).Times(1);
    EXPECT_CALL(focus_setter, set_focus_to(std::shared_ptr<msh::Session>())).Times(1);

    EXPECT_CALL(focus_sequence, default_focus()).WillOnce(Return((std::shared_ptr<msh::Session>())));

    auto session = session_manager.open_session("Visual Basic Studio", std::shared_ptr<me::EventSink>());

    std::shared_ptr<ms::Surface> surface;
    std::shared_ptr<msh::Surface> shell_surface;
    session->associate_surface(surface, shell_surface);

    session_manager.close_session(session);
}

TEST_F(SessionManagerSetup, new_applications_receive_focus)
{
    using namespace ::testing;
    std::shared_ptr<msh::Session> new_session;

    EXPECT_CALL(container, insert_session(_)).Times(1);
    EXPECT_CALL(focus_setter, set_focus_to(_)).WillOnce(SaveArg<0>(&new_session));

    auto session = session_manager.open_session("Visual Basic Studio", std::shared_ptr<me::EventSink>());
    EXPECT_EQ(session, new_session);
}

TEST_F(SessionManagerSetup, create_surface_for_session_forwards_and_then_focuses_session)
{
    using namespace ::testing;
    ON_CALL(surface_factory, create_surface(_,_,_,_)).WillByDefault(
        Return(std::make_shared<msh::Surface>(
            std::shared_ptr<ms::Surface>(),
            msh::a_surface())));

    // Once for session creation and once for surface creation
    {
        InSequence seq;

        EXPECT_CALL(focus_setter, set_focus_to(_)).Times(1); // Session creation
        EXPECT_CALL(surface_stack, create_surface(_,_))
            .Times(1)
            .WillOnce(Return(std::weak_ptr<ms::Surface>()));
        EXPECT_CALL(surface_factory, create_surface(_,_,_,_)).Times(1);
        EXPECT_CALL(focus_setter, set_focus_to(_)).Times(1); // Post Surface creation
    }

    auto session1 = session_manager.open_session("Weather Report", std::shared_ptr<me::EventSink>());
    session_manager.create_surface_for(session1, msh::a_surface());
}

namespace 
{

struct MockSessionListener : public msh::SessionListener
{
    virtual ~MockSessionListener() noexcept(true) {}
    MOCK_METHOD1(starting, void(std::shared_ptr<msh::Session> const&));
    MOCK_METHOD1(stopping, void(std::shared_ptr<msh::Session> const&));
    MOCK_METHOD1(focused, void(std::shared_ptr<msh::Session> const&));
    MOCK_METHOD0(unfocused, void());
};

struct SessionManagerSessionListenerSetup : public testing::Test
{
    //FIXME: should not construct the object outside the actual test, and def not in the test struct's constructor
    SessionManagerSessionListenerSetup()
      : session_manager(mt::fake_shared(surface_stack),
                        mt::fake_shared(surface_factory),
                        mt::fake_shared(container),
                        mt::fake_shared(focus_sequence),
                        mt::fake_shared(focus_setter),
                        mt::fake_shared(session_listener))
    {
    }

    MockSurfaceStackModel surface_stack;
    mtd::MockSurfaceFactory surface_factory;
    testing::NiceMock<MockSessionContainer> container;    // Inelegant but some tests need a stub
    testing::NiceMock<MockFocusSequence> focus_sequence;
    testing::NiceMock<mtd::MockFocusSetter> focus_setter; // Inelegant but some tests need a stub
    MockSessionListener session_listener;

    msh::SessionManager session_manager;
};
}

TEST_F(SessionManagerSessionListenerSetup, session_listener_is_notified_of_lifecycle_and_focus)
{
    using namespace ::testing;

    EXPECT_CALL(session_listener, starting(_)).Times(1);
    EXPECT_CALL(session_listener, focused(_)).Times(1);
    EXPECT_CALL(session_listener, stopping(_)).Times(1);
    EXPECT_CALL(session_listener, unfocused()).Times(1);

    EXPECT_CALL(focus_sequence, default_focus()).WillOnce(Return((std::shared_ptr<msh::Session>())));
    
    auto session = session_manager.open_session("XPlane", std::shared_ptr<me::EventSink>());
    session_manager.close_session(session);
}
