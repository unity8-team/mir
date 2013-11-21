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

#include "mir/compositor/buffer_stream.h"
#include "mir/frontend/session_mediator_report.h"
#include "src/server/frontend/session_mediator.h"
#include "src/server/frontend/resource_cache.h"
#include "mir/graphics/buffer_ipc_packer.h"
#include "src/server/scene/application_session.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/platform_ipc_package.h"
#include "src/server/scene/basic_surface.h"
#include "mir_test_doubles/mock_display.h"
#include "mir_test_doubles/mock_display_changer.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/null_event_sink.h"
#include "mir_test_doubles/null_display_changer.h"
#include "mir_test_doubles/mock_display.h"
#include "mir_test_doubles/mock_shell.h"
#include "mir_test_doubles/mock_frontend_surface.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_session.h"
#include "mir_test_doubles/stub_surface_builder.h"
#include "mir_test_doubles/stub_display_configuration.h"
#include "mir_test_doubles/stub_buffer_allocator.h"
#include "mir_test/display_config_matchers.h"
#include "mir_test/fake_shared.h"
#include "mir/frontend/event_sink.h"
#include "mir/shell/surface.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>

namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace ms = mir::scene;
namespace geom = mir::geometry;
namespace mp = mir::protobuf;
namespace msh = mir::shell;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{
struct StubConfig : public mtd::NullDisplayConfiguration
{
    StubConfig(std::shared_ptr<mg::DisplayConfigurationOutput> const& conf)
       : outputs{conf, conf}
    {
    }
    virtual void for_each_output(std::function<void(mg::DisplayConfigurationOutput const&)> f) const override
    {
        for(auto const& disp : outputs)
        {
            f(*disp);
        }
    }

    std::vector<std::shared_ptr<mg::DisplayConfigurationOutput>> outputs;
};

struct MockConfig : public mg::DisplayConfiguration
{
    MOCK_CONST_METHOD1(for_each_card, void(std::function<void(mg::DisplayConfigurationCard const&)>));
    MOCK_CONST_METHOD1(for_each_output, void(std::function<void(mg::DisplayConfigurationOutput const&)>));
    MOCK_METHOD5(configure_output, void(mg::DisplayConfigurationOutputId, bool, geom::Point, size_t, MirPowerMode));
};

}

namespace
{
class StubbedSession : public mtd::StubSession
{
public:
    StubbedSession() : last_surface_id{1}
    {
        using namespace ::testing;

        mock_surface = std::make_shared<mtd::MockFrontendSurface>();
        mock_surfaces[mf::SurfaceId{1}] = mock_surface;
        mock_buffer = std::make_shared<NiceMock<mtd::MockBuffer>>(geom::Size(), geom::Stride(), geom::PixelFormat());

        EXPECT_CALL(*mock_surface, size()).Times(AnyNumber()).WillRepeatedly(Return(geom::Size()));
        EXPECT_CALL(*mock_surface, pixel_format()).Times(AnyNumber()).WillRepeatedly(Return(geom::PixelFormat()));
        EXPECT_CALL(*mock_surface, advance_client_buffer()).Times(AnyNumber()).WillRepeatedly(Return(mock_buffer));

        EXPECT_CALL(*mock_surface, supports_input()).Times(AnyNumber()).WillRepeatedly(Return(true));
        EXPECT_CALL(*mock_surface, client_input_fd()).Times(AnyNumber()).WillRepeatedly(Return(testing_client_input_fd));
    }

    std::shared_ptr<mf::Surface> get_surface(mf::SurfaceId surface) const
    {
        return mock_surfaces.at(surface);
    }

    mf::SurfaceId create_surface(msh::SurfaceCreationParameters const& /* params */) override
    {
        using namespace ::testing;
        auto id = mf::SurfaceId{last_surface_id};
        if (last_surface_id != 1) {
            mock_surfaces[id] = std::make_shared<mtd::MockFrontendSurface>();

            EXPECT_CALL(*mock_surfaces[id], size()).Times(AnyNumber()).WillRepeatedly(Return(geom::Size()));
            EXPECT_CALL(*mock_surfaces[id], pixel_format()).Times(AnyNumber()).WillRepeatedly(Return(geom::PixelFormat()));
            EXPECT_CALL(*mock_surfaces[id], advance_client_buffer()).Times(AnyNumber()).WillRepeatedly(Return(mock_buffer));

            EXPECT_CALL(*mock_surfaces[id], supports_input()).Times(AnyNumber()).WillRepeatedly(Return(true));
            EXPECT_CALL(*mock_surfaces[id], client_input_fd()).Times(AnyNumber()).WillRepeatedly(Return(testing_client_input_fd));
        }
        last_surface_id++;
        return id;
    }

    void destroy_surface(mf::SurfaceId surface) override
    {
        mock_surfaces.erase(surface);
    }

    mtd::StubSurfaceBuilder surface_builder;
    std::shared_ptr<mtd::MockFrontendSurface> mock_surface;
    std::map<mf::SurfaceId, std::shared_ptr<mtd::MockFrontendSurface>> mock_surfaces;
    std::shared_ptr<mtd::MockBuffer> mock_buffer;
    static int const testing_client_input_fd;
    int last_surface_id;
};

int const StubbedSession::testing_client_input_fd{11};

class MockGraphicBufferAllocator : public mtd::StubBufferAllocator
{
public:
    MockGraphicBufferAllocator()
    {
        ON_CALL(*this, supported_pixel_formats())
            .WillByDefault(testing::Return(std::vector<geom::PixelFormat>()));
    }

    MOCK_METHOD0(supported_pixel_formats, std::vector<geom::PixelFormat>());
};

class MockPlatform : public mg::Platform
{
 public:
    MockPlatform()
    {
        using namespace testing;
        ON_CALL(*this, create_buffer_allocator(_))
            .WillByDefault(Return(std::shared_ptr<mg::GraphicBufferAllocator>()));
        ON_CALL(*this, create_display(_))
            .WillByDefault(Return(std::make_shared<mtd::NullDisplay>()));
        ON_CALL(*this, get_ipc_package())
            .WillByDefault(Return(std::make_shared<mg::PlatformIPCPackage>()));
    }

    MOCK_METHOD1(create_buffer_allocator, std::shared_ptr<mg::GraphicBufferAllocator>(std::shared_ptr<mg::BufferInitializer> const&));
    MOCK_METHOD1(create_display,
                 std::shared_ptr<mg::Display>(
                     std::shared_ptr<mg::DisplayConfigurationPolicy> const&));
    MOCK_METHOD0(get_ipc_package, std::shared_ptr<mg::PlatformIPCPackage>());
    MOCK_METHOD0(create_internal_client, std::shared_ptr<mg::InternalClient>());
    MOCK_CONST_METHOD2(fill_ipc_package, void(mg::BufferIPCPacker&, mg::Buffer const&));
    MOCK_CONST_METHOD0(egl_native_display, EGLNativeDisplayType());
};

struct SessionMediatorTest : public ::testing::Test
{
    SessionMediatorTest()
        : shell{std::make_shared<testing::NiceMock<mtd::MockShell>>()},
          graphics_platform{std::make_shared<testing::NiceMock<MockPlatform>>()},
          graphics_changer{std::make_shared<mtd::NullDisplayChanger>()},
          surface_pixel_formats{geom::PixelFormat::argb_8888, geom::PixelFormat::xrgb_8888},
          report{std::make_shared<mf::NullSessionMediatorReport>()},
          resource_cache{std::make_shared<mf::ResourceCache>()},
          mediator{shell, graphics_platform, graphics_changer,
                   surface_pixel_formats, report,
                   std::make_shared<mtd::NullEventSink>(),
                   resource_cache},
          stubbed_session{std::make_shared<StubbedSession>()},
          null_callback{google::protobuf::NewPermanentCallback(google::protobuf::DoNothing)}
    {
        using namespace ::testing;

        ON_CALL(*shell, open_session(_, _)).WillByDefault(Return(stubbed_session));
        ON_CALL(*shell, create_surface_for(_, _))
            .WillByDefault(WithArg<1>(Invoke(stubbed_session.get(), &StubbedSession::create_surface)));
    }

    std::shared_ptr<testing::NiceMock<mtd::MockShell>> const shell;
    std::shared_ptr<MockPlatform> const graphics_platform;
    std::shared_ptr<mf::DisplayChanger> const graphics_changer;
    std::vector<geom::PixelFormat> const surface_pixel_formats;
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
        mp::Surface surface_response;

        mediator.create_surface(nullptr, &request, &surface_response, null_callback.get());

        mp::SurfaceId surface = surface_response.id();
        mp::Buffer buffer_response;

        mediator.next_buffer(nullptr, &surface, &buffer_response, null_callback.get());

        mediator.release_surface(nullptr, &surface, nullptr, null_callback.get());
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

TEST_F(SessionMediatorTest, connect_packs_display_configuration)
{
    using namespace testing;
    geom::Size sz{1022, 2411};

    mtd::StubDisplayConfig config;

    auto mock_display = std::make_shared<mtd::MockDisplayChanger>();
    EXPECT_CALL(*mock_display, active_configuration())
        .Times(1)
        .WillOnce(Return(mt::fake_shared(config)));
    mf::SessionMediator mediator(
        shell, graphics_platform, mock_display,
        surface_pixel_formats, report,
        std::make_shared<mtd::NullEventSink>(),
        resource_cache);

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;
    connection.clear_platform();
    connection.clear_display_info();
    connection.clear_display_output();
    connection.clear_display_configuration();

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    EXPECT_THAT(connection.display_configuration(),
                mt::DisplayConfigMatches(std::cref(config)));
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
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{5}));

        mp::Surface surface_response;
        mp::SurfaceId buffer_request;
        mp::Buffer buffer_response[3];

        EXPECT_CALL(*graphics_platform, fill_ipc_package(_, _))
            .Times(2);

        mp::SurfaceParameters surface_request;
        mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());
        buffer_request = surface_response.id();
        mediator.next_buffer(nullptr, &buffer_request, &buffer_response[0], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request, &buffer_response[1], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request, &buffer_response[2], null_callback.get());
    }

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, session_with_multiple_surfaces_only_sends_needed_buffers)
{
    using namespace testing;

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    {
        EXPECT_CALL(*stubbed_session->mock_buffer, id())
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{9}))
            .WillOnce(Return(mg::BufferID{9}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{10}))
            .WillOnce(Return(mg::BufferID{10}))
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{4}))
            .WillOnce(Return(mg::BufferID{9}))
            .WillOnce(Return(mg::BufferID{9}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{5}))
            .WillOnce(Return(mg::BufferID{10}))
            .WillOnce(Return(mg::BufferID{10}));

        mp::Surface surface_response[2];
        mp::SurfaceId buffer_request[2];
        mp::Buffer buffer_response[6];

        EXPECT_CALL(*graphics_platform, fill_ipc_package(_, _))
            .Times(4);

        mp::SurfaceParameters surface_request;
        mediator.create_surface(nullptr, &surface_request, &surface_response[0], null_callback.get());
        mediator.create_surface(nullptr, &surface_request, &surface_response[1], null_callback.get());
        buffer_request[0] = surface_response[0].id();
        buffer_request[1] = surface_response[1].id();
        mediator.next_buffer(nullptr, &buffer_request[0], &buffer_response[0], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request[1], &buffer_response[1], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request[0], &buffer_response[2], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request[1], &buffer_response[3], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request[0], &buffer_response[4], null_callback.get());
        mediator.next_buffer(nullptr, &buffer_request[1], &buffer_response[5], null_callback.get());
    }

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}


TEST_F(SessionMediatorTest, buffer_resource_held_over_call)
{
    using namespace testing;

    auto stub_buffer1 = std::make_shared<mtd::StubBuffer>();
    auto stub_buffer2 = std::make_shared<mtd::StubBuffer>();

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mp::Surface surface_response;
    mp::SurfaceId buffer_request;
    mp::Buffer buffer_response;
    mp::SurfaceParameters surface_request;

    EXPECT_CALL(*stubbed_session->mock_surface, advance_client_buffer())
        .Times(2)
        .WillOnce(Return(stub_buffer1))
        .WillOnce(Return(stub_buffer2));
 
    auto refcount = stub_buffer1.use_count();
    mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());
    EXPECT_EQ(refcount+1, stub_buffer1.use_count());

    buffer_request = surface_response.id();

    auto refcount2 = stub_buffer2.use_count();
    mediator.next_buffer(nullptr, &buffer_request, &buffer_response, null_callback.get());
    EXPECT_EQ(refcount, stub_buffer1.use_count());
    EXPECT_EQ(refcount2+1, stub_buffer2.use_count());

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, buffer_resource_for_surface_held_over_operations_on_other_scene)
{
    using namespace testing;

    auto stub_buffer1 = std::make_shared<mtd::StubBuffer>();

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mp::SurfaceParameters surface_request;
    mp::Surface surface_response;

    /*
     * Note that the surface created by the first create_surface() call is
     * the pre-created stubbed_session->mock_surface. Further create_surface()
     * invocations create new surfaces in stubbed_session->mock_surfaces[].
     */
    EXPECT_CALL(*stubbed_session->mock_surface, advance_client_buffer())
        .WillOnce(Return(stub_buffer1));

    mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());
    auto refcount = stub_buffer1.use_count();

    /* Creating a new surface should not affect other surfaces' buffers */
    mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());
    EXPECT_EQ(refcount, stub_buffer1.use_count());

    mp::SurfaceId buffer_request{surface_response.id()};
    mp::Buffer buffer_response;

    /* Getting the next buffer of a surface should not affect other surfaces' buffers */
    mediator.next_buffer(nullptr, &buffer_request, &buffer_response, null_callback.get());
    EXPECT_EQ(refcount, stub_buffer1.use_count());

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediatorTest, display_config_request)
{
    using namespace testing;
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    bool used0 = false, used1 = true;
    geom::Point pt0{44,22}, pt1{3,2};
    size_t mode_index0 = 1, mode_index1 = 3; 
    mg::DisplayConfigurationOutputId id0{6}, id1{3};

    NiceMock<MockConfig> mock_display_config;
    mtd::StubDisplayConfig stub_display_config;
    auto mock_display_selector = std::make_shared<mtd::MockDisplayChanger>();

    Sequence seq;
    EXPECT_CALL(*mock_display_selector, active_configuration())
        .InSequence(seq)
        .WillOnce(Return(mt::fake_shared(mock_display_config))); 
    EXPECT_CALL(*mock_display_selector, active_configuration())
        .InSequence(seq)
        .WillOnce(Return(mt::fake_shared(mock_display_config))); 
    EXPECT_CALL(mock_display_config, configure_output(id0, used0, pt0, mode_index0,  mir_power_mode_on))
        .InSequence(seq);
    EXPECT_CALL(mock_display_config, configure_output(id1, used1, pt1, mode_index1, mir_power_mode_off))
        .InSequence(seq);
    EXPECT_CALL(*mock_display_selector, configure(_,_))
        .InSequence(seq);
    EXPECT_CALL(*mock_display_selector, active_configuration())
        .InSequence(seq)
        .WillOnce(Return(mt::fake_shared(stub_display_config)));
 
    mf::SessionMediator session_mediator{
        shell, graphics_platform, mock_display_selector,
        surface_pixel_formats, report,
        std::make_shared<mtd::NullEventSink>(), resource_cache};

    session_mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mp::DisplayConfiguration configuration_response;
    mp::DisplayConfiguration configuration; 
    auto disp0 = configuration.add_display_output();
    disp0->set_output_id(id0.as_value());
    disp0->set_used(used0);
    disp0->set_position_x(pt0.x.as_uint32_t());
    disp0->set_position_y(pt0.y.as_uint32_t());
    disp0->set_current_mode(mode_index0);
    disp0->set_power_mode(static_cast<uint32_t>(mir_power_mode_on));

    auto disp1 = configuration.add_display_output();
    disp1->set_output_id(id1.as_value());
    disp1->set_used(used1);
    disp1->set_position_x(pt1.x.as_uint32_t());
    disp1->set_position_y(pt1.y.as_uint32_t());
    disp1->set_current_mode(mode_index1);
    disp1->set_power_mode(static_cast<uint32_t>(mir_power_mode_off));

    session_mediator.configure_display(nullptr, &configuration,
                                       &configuration_response, null_callback.get());

    EXPECT_THAT(configuration_response, mt::DisplayConfigMatches(std::cref(stub_display_config)));

    session_mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}
