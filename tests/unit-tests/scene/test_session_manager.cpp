/*
 * Copyright © 2012-2015 Canonical Ltd.
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

#include "src/server/scene/session_manager.h"

#include "mir/scene/session.h"
#include "mir/scene/session_listener.h"
#include "mir/scene/null_session_listener.h"

#include "src/server/scene/basic_surface.h"
#include "src/server/scene/default_session_container.h"
#include "src/server/scene/session_event_sink.h"
#include "src/server/report/null_report_factory.h"

#include "mir/test/doubles/mock_surface_coordinator.h"
#include "mir/test/doubles/mock_session_listener.h"
#include "mir/test/doubles/stub_buffer_stream.h"
#include "mir/test/doubles/stub_buffer_stream_factory.h"
#include "mir/test/doubles/null_snapshot_strategy.h"
#include "mir/test/doubles/null_session_event_sink.h"
#include "mir/test/doubles/stub_surface_factory.h"
#include "mir/test/doubles/null_application_not_responding_detector.h"

#include "mir/test/fake_shared.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mf = mir::frontend;
namespace mi = mir::input;
namespace ms = mir::scene;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{
struct MockSessionContainer : public ms::SessionContainer
{
    MOCK_METHOD1(insert_session, void(std::shared_ptr<ms::Session> const&));
    MOCK_METHOD1(remove_session, void(std::shared_ptr<ms::Session> const&));
    MOCK_CONST_METHOD1(successor_of, std::shared_ptr<ms::Session>(std::shared_ptr<ms::Session> const&));
    MOCK_CONST_METHOD1(for_each, void(std::function<void(std::shared_ptr<ms::Session> const&)>));
    MOCK_METHOD0(lock, void());
    MOCK_METHOD0(unlock, void());
    ~MockSessionContainer() noexcept {}
};

struct MockSessionEventSink : public ms::SessionEventSink
{
    MOCK_METHOD1(handle_focus_change, void(std::shared_ptr<ms::Session> const& session));
    MOCK_METHOD0(handle_no_focus, void());
    MOCK_METHOD1(handle_session_stopping, void(std::shared_ptr<ms::Session> const& session));
};

struct SessionManagerSetup : public testing::Test
{
    void SetUp() override
    {
        using namespace ::testing;
        ON_CALL(container, successor_of(_)).WillByDefault(Return((std::shared_ptr<ms::Session>())));
    }

    std::shared_ptr<ms::Surface> dummy_surface = std::make_shared<ms::BasicSurface>(
        std::string("stub"),
        geom::Rectangle{{},{}},
        false,
        std::make_shared<mtd::StubBufferStream>(),
        std::shared_ptr<mi::InputChannel>(),
        std::shared_ptr<mi::InputSender>(),
        std::shared_ptr<mg::CursorImage>(),
        mir::report::null_scene_report());
    testing::NiceMock<mtd::MockSurfaceCoordinator> surface_coordinator;
    testing::NiceMock<MockSessionContainer> container;
    ms::NullSessionListener session_listener;
    mtd::StubBufferStreamFactory buffer_stream_factory;
    mtd::StubSurfaceFactory stub_surface_factory;

    ms::SessionManager session_manager{mt::fake_shared(surface_coordinator),
        mt::fake_shared(stub_surface_factory),
        mt::fake_shared(buffer_stream_factory),
        mt::fake_shared(container),
        std::make_shared<mtd::NullSnapshotStrategy>(),
        std::make_shared<mtd::NullSessionEventSink>(),
        mt::fake_shared(session_listener),
        std::make_shared<mtd::NullANRDetector>()};
};

}

TEST_F(SessionManagerSetup, open_and_close_session)
{
    using namespace ::testing;

    EXPECT_CALL(container, insert_session(_)).Times(1);
    EXPECT_CALL(container, remove_session(_)).Times(1);

    auto session = session_manager.open_session(__LINE__, "Visual Basic Studio", std::shared_ptr<mf::EventSink>());
    session_manager.close_session(session);
}

TEST_F(SessionManagerSetup, closing_session_removes_surfaces)
{
    using namespace ::testing;

    EXPECT_CALL(surface_coordinator, add_surface(_,_,_,_)).Times(1);

    EXPECT_CALL(container, insert_session(_)).Times(1);
    EXPECT_CALL(container, remove_session(_)).Times(1);

    auto session = session_manager.open_session(__LINE__, "Visual Basic Studio", std::shared_ptr<mf::EventSink>());
    session->create_surface(
        ms::a_surface().of_size(geom::Size{geom::Width{1024}, geom::Height{768}}),
        nullptr);

    session_manager.close_session(session);
}

namespace
{
struct SessionManagerSessionListenerSetup : public testing::Test
{
    void SetUp() override
    {
        using namespace ::testing;
        ON_CALL(container, successor_of(_)).WillByDefault(Return((std::shared_ptr<ms::Session>())));
    }

    mtd::MockSurfaceCoordinator surface_coordinator;
    testing::NiceMock<MockSessionContainer> container;
    testing::NiceMock<mtd::MockSessionListener> session_listener;
    mtd::StubSurfaceFactory stub_surface_factory;

    ms::SessionManager session_manager{
        mt::fake_shared(surface_coordinator),
        mt::fake_shared(stub_surface_factory),
        std::make_shared<mtd::StubBufferStreamFactory>(),
        mt::fake_shared(container),
        std::make_shared<mtd::NullSnapshotStrategy>(),
        std::make_shared<mtd::NullSessionEventSink>(),
        mt::fake_shared(session_listener),
        std::make_shared<mtd::NullANRDetector>()};
};
}

TEST_F(SessionManagerSessionListenerSetup, session_listener_is_notified_of_lifecycle)
{
    using namespace ::testing;

    EXPECT_CALL(session_listener, starting(_)).Times(1);
    EXPECT_CALL(session_listener, stopping(_)).Times(1);

    auto session = session_manager.open_session(__LINE__, "XPlane", std::shared_ptr<mf::EventSink>());
    session_manager.close_session(session);
}

namespace
{
struct SessionManagerSessionEventsSetup : public testing::Test
{
    void SetUp() override
    {
        using namespace ::testing;
        ON_CALL(container, successor_of(_)).WillByDefault(Return((std::shared_ptr<ms::Session>())));
    }

    mtd::MockSurfaceCoordinator surface_coordinator;
    testing::NiceMock<MockSessionContainer> container;    // Inelegant but some tests need a stub
    MockSessionEventSink session_event_sink;
    testing::NiceMock<mtd::MockSessionListener> session_listener;
    mtd::StubSurfaceFactory stub_surface_factory;

    ms::SessionManager session_manager{
        mt::fake_shared(surface_coordinator),
        mt::fake_shared(stub_surface_factory),
        std::make_shared<mtd::StubBufferStreamFactory>(),
        mt::fake_shared(container),
        std::make_shared<mtd::NullSnapshotStrategy>(),
        mt::fake_shared(session_event_sink),
        mt::fake_shared(session_listener),
        std::make_shared<mtd::NullANRDetector>()};
};
}

TEST_F(SessionManagerSessionEventsSetup, session_event_sink_is_notified_of_lifecycle)
{
    using namespace ::testing;

    auto session = session_manager.open_session(__LINE__, "XPlane", std::shared_ptr<mf::EventSink>());
    auto session1 = session_manager.open_session(__LINE__, "Bla", std::shared_ptr<mf::EventSink>());

    Mock::VerifyAndClearExpectations(&session_event_sink);

    EXPECT_CALL(session_event_sink, handle_focus_change(_)).Times(AnyNumber());
    EXPECT_CALL(session_event_sink, handle_no_focus()).Times(AnyNumber());

    InSequence s;
    EXPECT_CALL(session_event_sink, handle_session_stopping(_)).Times(1);
    EXPECT_CALL(session_event_sink, handle_session_stopping(_)).Times(1);

    session_manager.close_session(session1);
    session_manager.close_session(session);
}
