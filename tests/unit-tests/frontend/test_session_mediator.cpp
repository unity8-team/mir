/*
 * Copyright © 2012-2014 Canonical Ltd.
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
#include "src/server/frontend/session_mediator.h"
#include "src/server/report/null_report_factory.h"
#include "src/server/frontend/resource_cache.h"
#include "src/server/scene/application_session.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/input/cursor_images.h"
#include "src/server/scene/basic_surface.h"
#include "mir_test_doubles/mock_display.h"
#include "mir_test_doubles/mock_display_changer.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/null_event_sink.h"
#include "mir_test_doubles/null_display_changer.h"
#include "mir_test_doubles/mock_display.h"
#include "mir_test_doubles/mock_shell.h"
#include "mir_test_doubles/mock_frontend_surface.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_session.h"
#include "mir_test_doubles/stub_display_configuration.h"
#include "mir_test_doubles/stub_buffer_allocator.h"
#include "mir_test_doubles/null_screencast.h"
#include "mir_test/display_config_matchers.h"
#include "mir_test/fake_shared.h"
#include "mir/frontend/connector.h"
#include "mir/frontend/event_sink.h"

#include "gmock_set_arg.h"
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
namespace mr = mir::report;

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
    MOCK_METHOD1(for_each_output, void(std::function<void(mg::UserDisplayConfigurationOutput&)>));
};

struct MockConnector : public mf::Connector
{
public:
    void start() override {}
    void stop() override {}

    int client_socket_fd() const override { return 0; }

    MOCK_CONST_METHOD1(client_socket_fd, int (std::function<void(std::shared_ptr<mf::Session> const&)> const&));
};

class StubbedSession : public mtd::StubSession
{
public:
    StubbedSession() :
        mock_buffer{std::make_shared<mtd::StubBuffer>()},
        last_surface_id{0}
    {
    }

    std::shared_ptr<mf::Surface> get_surface(mf::SurfaceId surface) const
    {
        return mock_surfaces.at(surface);
    }

    std::shared_ptr<mtd::MockFrontendSurface> mock_surface_at(mf::SurfaceId id)
    {
        if (mock_surfaces.end() == mock_surfaces.find(id))
        {
            mock_surfaces[id] = 
                std::make_shared<testing::NiceMock<mtd::MockFrontendSurface>>(
                    mock_buffer, testing_client_input_fd);
        }
        return mock_surfaces.at(id);
    }

    mf::SurfaceId create_surface(ms::SurfaceCreationParameters const& /* params */) override
    {
        mf::SurfaceId id{last_surface_id};
        if (mock_surfaces.end() == mock_surfaces.find(id))
        {
            mock_surfaces[id] = std::make_shared<testing::NiceMock<mtd::MockFrontendSurface>>(
                mock_buffer, testing_client_input_fd);
        }
        last_surface_id++;
        return id;
    }

    void destroy_surface(mf::SurfaceId surface) override
    {
        mock_surfaces.erase(surface);
    }

    std::map<mf::SurfaceId, std::shared_ptr<mtd::MockFrontendSurface>> mock_surfaces;
    std::shared_ptr<mtd::StubBuffer> mock_buffer;
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
            .WillByDefault(testing::Return(std::vector<MirPixelFormat>()));
    }

    MOCK_METHOD0(supported_pixel_formats, std::vector<MirPixelFormat>());
};

class MockPlatform : public mg::Platform
{
 public:
    MockPlatform()
    {
        using namespace testing;
        ON_CALL(*this, create_buffer_allocator(_))
            .WillByDefault(Return(std::shared_ptr<mg::GraphicBufferAllocator>()));
        ON_CALL(*this, create_display(_,_,_))
            .WillByDefault(Return(std::make_shared<mtd::NullDisplay>()));
        ON_CALL(*this, get_ipc_package())
            .WillByDefault(Return(std::make_shared<mg::PlatformIPCPackage>()));
    }

    MOCK_METHOD1(create_buffer_allocator, std::shared_ptr<mg::GraphicBufferAllocator>(std::shared_ptr<mg::BufferInitializer> const&));
    MOCK_METHOD3(create_display,
                 std::shared_ptr<mg::Display>(
                     std::shared_ptr<mg::DisplayConfigurationPolicy> const&,
                     std::shared_ptr<mg::GLProgramFactory> const&,
                     std::shared_ptr<mg::GLConfig> const&));
    MOCK_METHOD0(get_ipc_package, std::shared_ptr<mg::PlatformIPCPackage>());
    MOCK_METHOD0(create_internal_client, std::shared_ptr<mg::InternalClient>());
    MOCK_CONST_METHOD3(fill_buffer_package,
        void(mg::BufferIPCPacker*, mg::Buffer const*, mg::BufferIpcMsgType));
    MOCK_CONST_METHOD0(egl_native_display, EGLNativeDisplayType());
};

struct StubScreencast : mtd::NullScreencast
{
    std::shared_ptr<mg::Buffer> capture(mf::ScreencastSessionId)
    {
        return mt::fake_shared(stub_buffer);
    }

    mtd::StubBuffer stub_buffer;
};

struct SessionMediator : public ::testing::Test
{
    SessionMediator()
        : shell{std::make_shared<testing::NiceMock<mtd::MockShell>>()},
          graphics_platform{std::make_shared<testing::NiceMock<MockPlatform>>()},
          graphics_changer{std::make_shared<mtd::NullDisplayChanger>()},
          surface_pixel_formats{mir_pixel_format_argb_8888, mir_pixel_format_xrgb_8888},
          report{mr::null_session_mediator_report()},
          resource_cache{std::make_shared<mf::ResourceCache>()},
          stub_screencast{std::make_shared<StubScreencast>()},
          stubbed_session{std::make_shared<StubbedSession>()},
          null_callback{google::protobuf::NewPermanentCallback(google::protobuf::DoNothing)},
          mediator{
            shell, graphics_platform, graphics_changer,
            surface_pixel_formats, report,
            std::make_shared<mtd::NullEventSink>(),
            resource_cache, stub_screencast, &connector, nullptr}
    {
        using namespace ::testing;

        ON_CALL(*shell, open_session(_, _, _)).WillByDefault(Return(stubbed_session));
    }

    MockConnector connector;
    std::shared_ptr<testing::NiceMock<mtd::MockShell>> const shell;
    std::shared_ptr<MockPlatform> const graphics_platform;
    std::shared_ptr<mf::DisplayChanger> const graphics_changer;
    std::vector<MirPixelFormat> const surface_pixel_formats;
    std::shared_ptr<mf::SessionMediatorReport> const report;
    std::shared_ptr<mf::ResourceCache> const resource_cache;
    std::shared_ptr<StubScreencast> const stub_screencast;
    std::shared_ptr<StubbedSession> const stubbed_session;
    std::unique_ptr<google::protobuf::Closure> null_callback;
    mf::SessionMediator mediator;

    mp::ConnectParameters connect_parameters;
    mp::Connection connection;
    mp::SurfaceParameters surface_parameters;
    mp::Surface surface_response;
    mp::SurfaceId surface_id_request;
    mp::Buffer buffer_response;
    mp::DRMMagic drm_request;
    mp::DRMAuthMagicStatus drm_response;

};
}

TEST_F(SessionMediator, disconnect_releases_session)
{
    using namespace ::testing;
    EXPECT_CALL(*shell, close_session(_))
        .Times(1);

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, connect_calls_connect_handler)
{
    using namespace ::testing;
    int connects_handled_count = 0;

    mf::ConnectionContext const context
    {
        [&](std::shared_ptr<mf::Session> const&) { ++connects_handled_count; },
        nullptr
    };

    mf::SessionMediator mediator{
        shell, graphics_platform, graphics_changer,
        surface_pixel_formats, report,
        std::make_shared<mtd::NullEventSink>(),
        resource_cache, stub_screencast, context, nullptr};

    EXPECT_THAT(connects_handled_count, Eq(0));

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    EXPECT_THAT(connects_handled_count, Eq(1));

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
    EXPECT_THAT(connects_handled_count, Eq(1));
}

TEST_F(SessionMediator, calling_methods_before_connect_throws)
{
    EXPECT_THROW({
        mediator.create_surface(nullptr, &surface_parameters, &surface_response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.next_buffer(nullptr, &surface_id_request, &buffer_response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.release_surface(nullptr, &surface_id_request, nullptr, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.drm_auth_magic(nullptr, &drm_request, &drm_response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
    }, std::logic_error);
}

TEST_F(SessionMediator, calling_methods_after_connect_works)
{
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    EXPECT_NO_THROW({
        mediator.create_surface(nullptr, &surface_parameters, &surface_response, null_callback.get());
        surface_id_request = surface_response.id();
        mediator.next_buffer(nullptr, &surface_id_request, &buffer_response, null_callback.get());
        mediator.release_surface(nullptr, &surface_id_request, nullptr, null_callback.get());
    });

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, calling_methods_after_disconnect_throws)
{
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());

    EXPECT_THROW({
        mediator.create_surface(nullptr, &surface_parameters, &surface_response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.next_buffer(nullptr, &surface_id_request, &buffer_response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.release_surface(nullptr, &surface_id_request, nullptr, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.drm_auth_magic(nullptr, &drm_request, &drm_response, null_callback.get());
    }, std::logic_error);

    EXPECT_THROW({
        mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
    }, std::logic_error);
}

//How does this test fail? consider removal
TEST_F(SessionMediator, can_reconnect_after_disconnect)
{
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
}

TEST_F(SessionMediator, connect_packs_display_configuration)
{
    using namespace testing;
    mtd::StubDisplayConfig config;
    auto mock_display = std::make_shared<NiceMock<mtd::MockDisplayChanger>>();
    ON_CALL(*mock_display, active_configuration())
        .WillByDefault(Return(mt::fake_shared(config)));

    mf::SessionMediator mediator(
        shell, graphics_platform, mock_display,
        surface_pixel_formats, report,
        std::make_shared<mtd::NullEventSink>(),
        resource_cache, std::make_shared<mtd::NullScreencast>(),
        nullptr, nullptr);
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    EXPECT_THAT(connection.display_configuration(), mt::DisplayConfigMatches(std::cref(config)));
}

TEST_F(SessionMediator, creating_surface_packs_response_with_input_fds)
{
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.create_surface(nullptr, &surface_parameters, &surface_response, null_callback.get());
    ASSERT_THAT(surface_response.fd().size(), testing::Ge(1));
    EXPECT_EQ(StubbedSession::testing_client_input_fd, surface_response.fd(0));

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, no_input_channel_returns_no_fds)
{
    using namespace testing;

    auto surface = stubbed_session->mock_surface_at(mf::SurfaceId{0});
    EXPECT_CALL(*surface, supports_input())
        .WillOnce(Return(false));
    EXPECT_CALL(*surface, client_input_fd())
        .Times(0);

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.create_surface(nullptr, &surface_parameters, &surface_response, null_callback.get());
    EXPECT_THAT(surface_response.fd().size(), Eq(0));

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, session_only_sends_mininum_information_for_buffers)
{
    using namespace testing;
    mtd::StubBuffer buffer1;
    mtd::StubBuffer buffer2;
    mp::Buffer buffer_response[3];
    auto surface = stubbed_session->mock_surface_at(mf::SurfaceId{0});
    Sequence seq;

    //create
    EXPECT_CALL(*surface, swap_buffers(_, _))
        .InSequence(seq)
        .WillOnce(InvokeArgument<1>(&buffer2));
    EXPECT_CALL(*graphics_platform, fill_buffer_package(_, &buffer2, mg::BufferIpcMsgType::full_msg))
        .InSequence(seq);
    //swap1
    EXPECT_CALL(*surface, swap_buffers(&buffer2, _))
        .InSequence(seq)
        .WillOnce(InvokeArgument<1>(&buffer1));
    EXPECT_CALL(*graphics_platform, fill_buffer_package(_, &buffer1, mg::BufferIpcMsgType::full_msg))
        .InSequence(seq);
    //swap2
    EXPECT_CALL(*surface, swap_buffers(&buffer1, _))
        .InSequence(seq)
        .WillOnce(InvokeArgument<1>(&buffer2));
    EXPECT_CALL(*graphics_platform, fill_buffer_package(_, &buffer2, mg::BufferIpcMsgType::update_msg))
        .InSequence(seq);
    //swap3
    EXPECT_CALL(*surface, swap_buffers(&buffer2, _))
        .InSequence(seq)
        .WillOnce(InvokeArgument<1>(&buffer1));
    EXPECT_CALL(*graphics_platform, fill_buffer_package(_, &buffer1, mg::BufferIpcMsgType::update_msg))
        .InSequence(seq);

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.create_surface(nullptr, &surface_parameters, &surface_response, null_callback.get());
    surface_id_request = surface_response.id();
    mediator.next_buffer(nullptr, &surface_id_request, &buffer_response[0], null_callback.get());
    mediator.next_buffer(nullptr, &surface_id_request, &buffer_response[1], null_callback.get());
    mediator.next_buffer(nullptr, &surface_id_request, &buffer_response[2], null_callback.get());
    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, session_with_multiple_surfaces_only_sends_needed_buffers)
{
    using namespace testing;
    mtd::StubBuffer buffer[4];
    mf::SurfaceId first_id{0};
    mf::SurfaceId second_id{1};

    mp::SurfaceId buffer_request[2];
    buffer_request[0].set_value(first_id.as_value());
    buffer_request[1].set_value(second_id.as_value());

    auto surface1 = stubbed_session->mock_surface_at(first_id);
    auto surface2 = stubbed_session->mock_surface_at(second_id);
    EXPECT_CALL(*surface1, swap_buffers(_,_))
        .WillOnce(InvokeArgument<1>(&buffer[0]))
        .WillOnce(InvokeArgument<1>(&buffer[1]))
        .WillOnce(InvokeArgument<1>(&buffer[0]))
        .WillOnce(InvokeArgument<1>(&buffer[1]));
    EXPECT_CALL(*surface2, swap_buffers(_,_))
        .WillOnce(InvokeArgument<1>(&buffer[2]))
        .WillOnce(InvokeArgument<1>(&buffer[3]))
        .WillOnce(InvokeArgument<1>(&buffer[2]))
        .WillOnce(InvokeArgument<1>(&buffer[3]));

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mp::Surface surface_response[2];
    mp::Buffer buffer_response[6];

    EXPECT_CALL(*graphics_platform, fill_buffer_package(_,_,mg::BufferIpcMsgType::full_msg))
        .Times(4);
    EXPECT_CALL(*graphics_platform, fill_buffer_package(_,_,mg::BufferIpcMsgType::update_msg))
        .Times(4);

    mediator.create_surface(nullptr, &surface_parameters, &surface_response[0], null_callback.get());
    mediator.create_surface(nullptr, &surface_parameters, &surface_response[1], null_callback.get());
    mediator.next_buffer(nullptr, &buffer_request[0], &buffer_response[0], null_callback.get());
    mediator.next_buffer(nullptr, &buffer_request[1], &buffer_response[1], null_callback.get());
    mediator.next_buffer(nullptr, &buffer_request[0], &buffer_response[2], null_callback.get());
    mediator.next_buffer(nullptr, &buffer_request[1], &buffer_response[3], null_callback.get());
    mediator.next_buffer(nullptr, &buffer_request[0], &buffer_response[4], null_callback.get());
    mediator.next_buffer(nullptr, &buffer_request[1], &buffer_response[5], null_callback.get());

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, buffer_resource_for_surface_unaffected_by_other_surfaces)
{
    using namespace testing;
    mtd::StubBuffer buffer;
    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());
    mp::SurfaceParameters surface_request;
    mp::Surface surface_response;

    auto surface1 = stubbed_session->mock_surface_at(mf::SurfaceId{0});
    EXPECT_CALL(*surface1, swap_buffers(_, _))
        .WillOnce(InvokeArgument<1>(&buffer));

    mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());
    mp::SurfaceId our_surface{surface_response.id()};

    /* Creating a new surface should not affect our surfaces' buffers */
    EXPECT_CALL(*surface1, swap_buffers(_, _)).Times(0);
    mediator.create_surface(nullptr, &surface_request, &surface_response, null_callback.get());

    mp::SurfaceId new_surface{surface_response.id()};
    mp::Buffer buffer_response;

    /* Getting the next buffer of new surface should not affect our surfaces' buffers */
    mediator.next_buffer(nullptr, &new_surface, &buffer_response, null_callback.get());

    /* Getting the next buffer of our surface should post the original */
    EXPECT_CALL(*surface1, swap_buffers(Eq(&buffer), _)).Times(1);

    mediator.next_buffer(nullptr, &our_surface, &buffer_response, null_callback.get());
    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, display_config_request)
{
    using namespace testing;
    mp::ConnectParameters connect_parameters;
    mp::Connection connection;

    bool used0 = false, used1 = true;
    geom::Point pt0{44,22}, pt1{3,2};
    size_t mode_index0 = 1, mode_index1 = 3;
    MirPixelFormat format0{mir_pixel_format_invalid};
    MirPixelFormat format1{mir_pixel_format_argb_8888};
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
    EXPECT_CALL(*mock_display_selector, configure(_,_))
        .InSequence(seq);
    EXPECT_CALL(*mock_display_selector, active_configuration())
        .InSequence(seq)
        .WillOnce(Return(mt::fake_shared(stub_display_config)));

    mf::SessionMediator mediator{
        shell, graphics_platform, mock_display_selector,
        surface_pixel_formats, report,
        std::make_shared<mtd::NullEventSink>(),
        resource_cache, stub_screencast, &connector, nullptr};

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mp::DisplayConfiguration configuration_response;
    mp::DisplayConfiguration configuration;
    auto disp0 = configuration.add_display_output();
    disp0->set_output_id(id0.as_value());
    disp0->set_used(used0);
    disp0->set_position_x(pt0.x.as_uint32_t());
    disp0->set_position_y(pt0.y.as_uint32_t());
    disp0->set_current_mode(mode_index0);
    disp0->set_current_format(format0);
    disp0->set_power_mode(static_cast<uint32_t>(mir_power_mode_on));
    disp0->set_orientation(mir_orientation_left);

    auto disp1 = configuration.add_display_output();
    disp1->set_output_id(id1.as_value());
    disp1->set_used(used1);
    disp1->set_position_x(pt1.x.as_uint32_t());
    disp1->set_position_y(pt1.y.as_uint32_t());
    disp1->set_current_mode(mode_index1);
    disp1->set_current_format(format1);
    disp1->set_power_mode(static_cast<uint32_t>(mir_power_mode_off));
    disp1->set_orientation(mir_orientation_inverted);

    mediator.configure_display(nullptr, &configuration,
                                       &configuration_response, null_callback.get());

    EXPECT_THAT(configuration_response, mt::DisplayConfigMatches(std::cref(stub_display_config)));

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}

TEST_F(SessionMediator, fully_packs_buffer_for_create_screencast)
{
    using namespace testing;

    mp::ScreencastParameters screencast_parameters;
    mp::Screencast screencast;
    auto const& stub_buffer = stub_screencast->stub_buffer;

    EXPECT_CALL(*graphics_platform, fill_buffer_package(_, &stub_buffer, _));

    mediator.create_screencast(nullptr, &screencast_parameters,
                               &screencast, null_callback.get());
    EXPECT_EQ(stub_buffer.id().as_uint32_t(), screencast.buffer().buffer_id());
}

TEST_F(SessionMediator, partially_packs_buffer_for_screencast_buffer)
{
    using namespace testing;

    mp::ScreencastId screencast_id;
    mp::Buffer protobuf_buffer;
    auto const& stub_buffer = stub_screencast->stub_buffer;

    EXPECT_CALL(*graphics_platform,
        fill_buffer_package(
            _, &stub_buffer, mg::BufferIpcMsgType::update_msg))
        .Times(1);

    mediator.screencast_buffer(nullptr, &screencast_id,
                               &protobuf_buffer, null_callback.get());
    EXPECT_EQ(stub_buffer.id().as_uint32_t(), protobuf_buffer.buffer_id());
}

TEST_F(SessionMediator, prompt_provider_fds_allocated_by_connector)
{
    using namespace ::testing;
    int const fd_count{11};
    int const dummy_fd{__LINE__};
    mp::SocketFDRequest request;
    mp::SocketFD response;
    request.set_number(fd_count);

    EXPECT_CALL(connector, client_socket_fd(_))
        .Times(fd_count)
        .WillRepeatedly(Return(dummy_fd));

    mediator.connect(nullptr, &connect_parameters, &connection, null_callback.get());

    mediator.new_fds_for_prompt_providers(nullptr, &request, &response, null_callback.get());
    EXPECT_THAT(response.fd_size(), Eq(fd_count));

    mediator.disconnect(nullptr, nullptr, nullptr, null_callback.get());
}
