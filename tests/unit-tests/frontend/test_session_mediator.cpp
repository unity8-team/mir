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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/surfaces/buffer_bundle.h"
#include "mir/compositor/graphic_buffer_allocator.h"
#include "mir/frontend/session_mediator_report.h"
#include "mir/frontend/session_mediator.h"
#include "mir/frontend/resource_cache.h"
#include "mir/shell/application_session.h"
#include "mir/graphics/display.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/surfaces/surface.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/mock_shell.h"
#include "mir_test_doubles/mock_surface.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_session.h"
#include "mir_test_doubles/stub_surface_builder.h"
#include "mir_test_doubles/stub_platform.h"
#include "mir_test/fake_shared.h"
#include "mir/events/event_sink.h"
#include "mir/shell/surface.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>

namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace ms = mir::surfaces;
namespace geom = mir::geometry;
namespace mp = mir::protobuf;
namespace msh = mir::shell;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{

class StubbedSession : public mtd::StubSession
{
public:
    StubbedSession()
    {
        using namespace ::testing;

        mock_surface = std::make_shared<mtd::MockSurface>(mt::fake_shared(surface_builder));
        mock_buffer = std::make_shared<NiceMock<mtd::MockBuffer>>(geom::Size(), geom::Stride(), geom::PixelFormat());

        EXPECT_CALL(*mock_surface, size()).Times(AnyNumber()).WillRepeatedly(Return(geom::Size()));
        EXPECT_CALL(*mock_surface, pixel_format()).Times(AnyNumber()).WillRepeatedly(Return(geom::PixelFormat()));
        EXPECT_CALL(*mock_surface, client_buffer()).Times(AnyNumber()).WillRepeatedly(Return(mock_buffer));
        EXPECT_CALL(*mock_surface, advance_client_buffer()).Times(AnyNumber());

        EXPECT_CALL(*mock_surface, supports_input()).Times(AnyNumber()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_surface, client_input_fd()).Times(AnyNumber()).WillRepeatedly(Return(testing_client_input_fd));
    }

    std::shared_ptr<mf::Surface> get_surface(mf::SurfaceId /* surface */) const
    {
        return mock_surface;
    }

    mtd::StubSurfaceBuilder surface_builder;
    std::shared_ptr<mtd::MockSurface> mock_surface;
    std::shared_ptr<mtd::MockBuffer> mock_buffer;
    static int const testing_client_input_fd;
};

int const StubbedSession::testing_client_input_fd{11};

class MockPlatform : public mg::Platform
{
 public:
    MockPlatform()
    {
        using namespace testing;
        ON_CALL(*this, create_buffer_allocator(_))
            .WillByDefault(Return(std::shared_ptr<mc::GraphicBufferAllocator>()));
        ON_CALL(*this, create_display())
            .WillByDefault(Return(std::make_shared<mtd::NullDisplay>()));
        ON_CALL(*this, get_ipc_package())
            .WillByDefault(Return(std::make_shared<mg::PlatformIPCPackage>()));
    }

    MOCK_METHOD1(create_buffer_allocator, std::shared_ptr<mc::GraphicBufferAllocator>(std::shared_ptr<mg::BufferInitializer> const&));
    MOCK_METHOD0(create_display, std::shared_ptr<mg::Display>());
    MOCK_METHOD0(get_ipc_package, std::shared_ptr<mg::PlatformIPCPackage>());
    MOCK_METHOD0(create_internal_client, std::shared_ptr<mg::InternalClient>());
    MOCK_CONST_METHOD2(fill_ipc_package, void(std::shared_ptr<mc::BufferIPCPacker> const&,
                                              std::shared_ptr<mc::Buffer> const&));
    MOCK_METHOD0(supported_pixel_formats, std::vector<geom::PixelFormat>());

};

class NullEventSink : public mir::events::EventSink
{
public:
    void handle_event(MirEvent const& ) override {}
};

struct SessionMediatorTest : public ::testing::Test
{
    SessionMediatorTest()
        : shell{std::make_shared<testing::NiceMock<mtd::MockShell>>()},
          graphics_platform{std::make_shared<MockPlatform>()},
          graphics_display{std::make_shared<mtd::NullDisplay>()},
          report{std::make_shared<mf::NullSessionMediatorReport>()},
          resource_cache{std::make_shared<mf::ResourceCache>()},
          mediator{shell, graphics_platform, graphics_display, report
                   std::make_shared<NullEventSink>(),
                   resource_cache},
          stubbed_session{std::make_shared<StubbedSession>()},
          null_callback{google::protobuf::NewPermanentCallback(google::protobuf::DoNothing)}
    {
        using namespace ::testing;

        ON_CALL(*shell, open_session(_, _)).WillByDefault(Return(stubbed_session));
        ON_CALL(*shell, create_surface_for(_, _)).WillByDefault(Return(mf::SurfaceId{1}));
    }

    std::shared_ptr<testing::NiceMock<mtd::MockShell>> const shell;
    std::shared_ptr<MockPlatform> const graphics_platform;
    std::shared_ptr<mg::Display> const graphics_display;
    std::shared_ptr<mf::SessionMediatorReport> const report;
    std::shared_ptr<mf::ResourceCache> const resource_cache;
    mf::SessionMediator mediator;
    std::shared_ptr<StubbedSession> const stubbed_session;

    std::unique_ptr<google::protobuf::Closure> null_callback;
};
}

TEST_F(SessionMediatorTest, disconnect_releases_session)
{
    using namespace ::testing;

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    EXPECT_CALL(*shell, close_session(_)).Times(1);

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, calling_methods_before_connect_throws)
{
    EXPECT_THROW({
        mp::SurfaceParameters request;
        mp::Surface response;

        mediator.create_surface(nullptr, &request, &response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mp::SurfaceId request;
        mp::Buffer response;

        mediator.next_buffer(nullptr, &request, &response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mp::SurfaceId request;

        mediator.release_surface(nullptr, &request, nullptr, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mp::DRMMagic request;
        mp::DRMAuthMagicStatus response;

        mediator.drm_auth_magic(nullptr, &request, &response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
    }, std::logic_error);
}

TEST_F(SessionMediatorTest, calling_methods_after_connect_works)
{
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    {
        mp::SurfaceParameters request;
        mp::Surface response;

        mediator.create_surface(nullptr, &request, &response, null_callback.get());
    }
    {
        mp::SurfaceId request;
        mp::Buffer response;

        mediator.next_buffer(nullptr, &request, &response, null_callback.get());
    }
    {
        mp::SurfaceId request;

        mediator.release_surface(nullptr, &request, nullptr, null_callback.get());
    }

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, calling_methods_after_disconnect_throws)
{
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());

    EXPECT_THROW({
        mp::SurfaceParameters surface_parameters;
        mp::Surface surface;

        mediator.create_surface(nullptr, &surface_parameters, &surface, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mp::SurfaceId request;
        mp::Buffer response;

        mediator.next_buffer(nullptr, &request, &response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mp::SurfaceId request;

        mediator.release_surface(nullptr, &request, nullptr, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mp::DRMMagic request;
        mp::DRMAuthMagicStatus response;

        mediator.drm_auth_magic(nullptr, &request, &response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
    }, std::logic_error);
}

TEST_F(SessionMediatorTest, can_reconnect_after_disconnect)
{
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
}

TEST_F(SessionMediatorTest, connect_queries_supported_pixel_formats)
{
    using namespace testing;

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    std::vector<geom::PixelFormat> const pixel_formats{
        geom::PixelFormat::bgr_888,
        geom::PixelFormat::abgr_8888,
        geom::PixelFormat::xbgr_8888
    };

    EXPECT_CALL(*graphics_platform, supported_pixel_formats())
        .WillOnce(Return(pixel_formats));

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    auto info = connection.display_info();

    ASSERT_EQ(pixel_formats.size(), static_cast<size_t>(info.supported_pixel_format_size()));

    for (size_t i = 0; i < pixel_formats.size(); ++i)
    {
        EXPECT_EQ(pixel_formats[i], static_cast<geom::PixelFormat>(info.supported_pixel_format(i)))
            << "i = " << i;
    }
}

TEST_F(SessionMediatorTest, creating_surface_packs_response_with_input_fds)
{
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    {
        mp::SurfaceParameters request;
        mp::Surface response;

        mediator.create_surface(nullptr, &request, &response, null_callback.get());
        EXPECT_EQ(StubbedSession::testing_client_input_fd, response.fd(0));
    }

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, no_input_channel_is_nonfatal)
{
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;
    EXPECT_CALL(*stubbed_session->mock_surface, supports_input())
        .Times(1)
        .WillOnce(testing::Return(false));
    EXPECT_CALL(*stubbed_session->mock_surface, client_input_fd())
        .Times(0);

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    {
        mp::SurfaceParameters request;
        mp::Surface response;

        mediator.create_surface(nullptr, &request, &response, null_callback.get());
    }

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, session_only_sends_needed_buffers)
{
    using namespace testing;

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    {
        EXPECT_CALL(*stubbed_session->mock_buffer, id())
            .WillOnce(Return(mc::BufferID{4}))
            .WillOnce(Return(mc::BufferID{5}))
            .WillOnce(Return(mc::BufferID{4}))
            .WillOnce(Return(mc::BufferID{5}));

        mp::Surface surface_response;
        mp::SurfaceId buffer_request;
        mp::Buffer buffer_response[3];

        std::shared_ptr<mc::Buffer> tmp_buffer = stubbed_session->mock_buffer;
        EXPECT_CALL(*graphics_platform, fill_ipc_package(_, tmp_buffer))
            .Times(2);

        mp::SurfaceParameters surface_request;
        mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request, &buffer_response[0], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request, &buffer_response[1], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request, &buffer_response[2], null_callback.get());
    }

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}
