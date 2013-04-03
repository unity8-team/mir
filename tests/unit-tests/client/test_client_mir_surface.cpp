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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir_protobuf.pb.h"
#include "mir_toolkit/mir_client_library.h"
#include "src/client/mir_logger.h"
#include "src/client/client_buffer.h"
#include "src/client/client_buffer_depository.h"
#include "src/client/client_buffer_factory.h"
#include "src/client/client_platform.h"
#include "src/client/client_platform_factory.h"
#include "src/client/mir_surface.h"
#include "src/client/mir_connection.h"
#include "mir/frontend/resource_cache.h"

#include "mir_test/test_protobuf_server.h"
#include "mir_test/stub_server_tool.h"
#include "mir_test/test_protobuf_client.h"
#include "mir_test/gmock_fixes.h"

#include <cstring>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mcl = mir::client;
namespace mp = mir::protobuf;
namespace geom = mir::geometry;

namespace mir
{
namespace test
{

struct MockServerPackageGenerator : public StubServerTool
{
    MockServerPackageGenerator()
     : global_buffer_id(0)
    {
        generate_unique_buffer();
        width_sent  = 891;
        height_sent = 458;
        pf_sent = mir_pixel_format_abgr_8888;
    }

    void create_surface(google::protobuf::RpcController*,
                 const mir::protobuf::SurfaceParameters* request,
                 mir::protobuf::Surface* response,
                 google::protobuf::Closure* done)
    {
        create_surface_response(response);
        surface_name = request->surface_name();
        done->Run();
    }

    void next_buffer(
        ::google::protobuf::RpcController* /*controller*/,
        ::mir::protobuf::SurfaceId const* /*request*/,
        ::mir::protobuf::Buffer* response,
        ::google::protobuf::Closure* done)
    {
        create_buffer_response(response);
        done->Run();
    }

/* helpers */
    void generate_unique_buffer()
    {
        global_buffer_id++;

        int num_fd = 2, num_data = 8;
        for (auto i=0; i<num_fd; i++)
        {
            server_package.fd[i] = global_buffer_id * i;
        }
        for (auto i=0; i<num_data; i++)
        {
            server_package.data[i] = (global_buffer_id + i) * 2;
        }
        server_package.stride = 66;
    }

    MirBufferPackage server_package;

    int width_sent;
    int height_sent;
    int pf_sent;

    private:
    int global_buffer_id;

    void create_buffer_response(mir::protobuf::Buffer* response)
    {
        response->set_buffer_id(global_buffer_id);

        /* assemble buffers */
        response->set_fds_on_side_channel(1);
        for (int i=0; i< server_package.data_items; i++)
        {
            response->add_data(server_package.data[i]);
        }
        for (int i=0; i< server_package.fd_items; i++)
        {
            response->add_fd(server_package.fd[i]);
        }

        response->set_stride(server_package.stride);

        generate_unique_buffer();
    }

    void create_surface_response(mir::protobuf::Surface* response)
    {
        response->mutable_id()->set_value(2);
        response->set_width(width_sent);
        response->set_height(height_sent);
        response->set_pixel_format(pf_sent);
        create_buffer_response(response->mutable_buffer());
    }
};

struct MockBuffer : public mcl::ClientBuffer
{
    explicit MockBuffer(std::shared_ptr<MirBufferPackage> const& contents)
    {
        using namespace testing;

        auto buffer_package = std::make_shared<MirBufferPackage>();
        *buffer_package = *contents;
        ON_CALL(*this, get_buffer_package())
            .WillByDefault(Return(buffer_package));
    }

    MOCK_METHOD0(secure_for_cpu_write, std::shared_ptr<mcl::MemoryRegion>());
    MOCK_CONST_METHOD0(size, geom::Size());
    MOCK_CONST_METHOD0(stride, geom::Stride());
    MOCK_CONST_METHOD0(pixel_format, geom::PixelFormat());
    MOCK_CONST_METHOD0(age, uint32_t());
    MOCK_METHOD0(increment_age, void());
    MOCK_METHOD0(mark_as_submitted, void());
    MOCK_CONST_METHOD0(get_buffer_package, std::shared_ptr<MirBufferPackage>());
    MOCK_METHOD0(get_native_handle, MirNativeBuffer());
};

struct MockClientBufferFactory : public mcl::ClientBufferFactory
{
    MockClientBufferFactory()
    {
        using namespace testing;

        emptybuffer=std::make_shared<NiceMock<MockBuffer>>(std::make_shared<MirBufferPackage>());

        ON_CALL(*this, create_buffer(_,_,_))
            .WillByDefault(DoAll(SaveArg<0>(&current_package),
                                 InvokeWithoutArgs([this] () {this->current_buffer = std::make_shared<NiceMock<MockBuffer>>(current_package);}),
                                 ReturnPointee(&current_buffer)));
    }

    MOCK_METHOD3(create_buffer,
                 std::shared_ptr<mcl::ClientBuffer>(std::shared_ptr<MirBufferPackage> const&,
                                                    geom::Size, geom::PixelFormat));

    std::shared_ptr<MirBufferPackage> current_package;
    std::shared_ptr<mcl::ClientBuffer> current_buffer;
    std::shared_ptr<mcl::ClientBuffer> emptybuffer;
};


struct StubClientPlatform : public mcl::ClientPlatform
{
    std::shared_ptr<mcl::ClientBufferFactory> create_buffer_factory()
    {
        return std::shared_ptr<MockClientBufferFactory>();
    }

    std::shared_ptr<EGLNativeWindowType> create_egl_native_window(mcl::ClientSurface* /*surface*/)
    {
        return std::shared_ptr<EGLNativeWindowType>();
    }

    std::shared_ptr<EGLNativeDisplayType> create_egl_native_display()
    {
        return std::shared_ptr<EGLNativeDisplayType>();
    }
};

struct StubClientPlatformFactory : public mcl::ClientPlatformFactory
{
    std::shared_ptr<mcl::ClientPlatform> create_client_platform(mcl::ClientContext* /*context*/)
    {
        return std::make_shared<StubClientPlatform>();
    }
};

}
}

namespace mt = mir::test;

void connected_callback(MirConnection* /*connection*/, void * /*client_context*/)
{
}

struct CallBack
{
    void msg() {}
};

struct MirClientSurfaceTest : public testing::Test
{
    void SetUp()
    {
        mock_server_tool = std::make_shared<mt::MockServerPackageGenerator>();
        test_server = std::make_shared<mt::TestProtobufServer>("./test_socket_surface", mock_server_tool);

        test_server->comm->start();

        mock_buffer_factory = std::make_shared<mt::MockClientBufferFactory>();

        params = MirSurfaceParameters{"test", 33, 45, mir_pixel_format_abgr_8888,
                                      mir_buffer_usage_hardware};

        /* connect dummy server */
        connect_parameters.set_application_name("test");

        /* connect client */
        logger = std::make_shared<mcl::ConsoleLogger>();
        platform_factory = std::make_shared<mt::StubClientPlatformFactory>();
        channel = mcl::make_rpc_channel("./test_socket_surface", logger);
        connection = std::make_shared<MirConnection>(channel, logger, platform_factory);
        MirWaitHandle* wait_handle = connection->connect("MirClientSurfaceTest",
                                                         connected_callback, 0);
        wait_handle->wait_for_result();
        client_comm_channel = std::make_shared<mir::protobuf::DisplayServer::Stub>(channel.get());
    }

    void TearDown()
    {
        test_server.reset();
    }

    std::shared_ptr<google::protobuf::RpcChannel> channel;
    std::shared_ptr<mcl::Logger> logger;
    std::shared_ptr<mcl::ClientPlatformFactory> platform_factory;
    std::shared_ptr<MirConnection> connection;

    MirSurfaceParameters params;
    std::shared_ptr<mt::MockClientBufferFactory> mock_buffer_factory;

    mir::protobuf::Connection response;
    mir::protobuf::ConnectParameters connect_parameters;

    std::shared_ptr<mt::TestProtobufServer> test_server;
    std::shared_ptr<mt::TestProtobufClient> client_tools;
    std::shared_ptr<mt::MockServerPackageGenerator> mock_server_tool;

    CallBack callback;

    std::shared_ptr<mir::protobuf::DisplayServer::Stub> client_comm_channel;
};

void empty_callback(MirSurface*, void*) { }
TEST_F(MirClientSurfaceTest, client_buffer_created_on_surface_creation)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1);

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);

    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();
}

namespace
{
void empty_surface_callback(MirSurface*, void*) {}
}

TEST_F(MirClientSurfaceTest, client_buffer_created_on_next_buffer)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1);

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);

    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1);
    auto buffer_wait_handle = surface->next_buffer(&empty_surface_callback, (void*) NULL);
    buffer_wait_handle->wait_for_result();
}

MATCHER_P(BufferPackageMatches, package, "")
{
    // Can't simply use memcmp() on the whole struct because age is not sent over the wire
    if (package.data_items != arg.data_items)
        return false;
    if (package.fd_items != arg.fd_items)
        return false;
    if (memcmp(package.data, arg.data, sizeof(package.data[0]) * package.data_items))
        return false;
    if (memcmp(package.fd, arg.fd, sizeof(package.fd[0]) * package.fd_items))
        return false;
    if (package.stride != arg.stride)
        return false;
    return true;
}

TEST_F(MirClientSurfaceTest, client_buffer_uses_ipc_message_from_server_on_create)
{
    using namespace testing;

    std::shared_ptr<MirBufferPackage> submitted_package;
    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<0>(&submitted_package),
                        Return(mock_buffer_factory->emptybuffer)));

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    /* check for same contents */
    EXPECT_THAT(*submitted_package, BufferPackageMatches(mock_server_tool->server_package));
}

TEST_F(MirClientSurfaceTest, message_width_used_in_buffer_creation )
{
    using namespace testing;

    geom::Size sz;
    std::shared_ptr<MirBufferPackage> submitted_package;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<1>(&sz),
                        Return(mock_buffer_factory->emptybuffer)));

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_EQ(sz.width, (unsigned int) mock_server_tool->width_sent);
}

TEST_F(MirClientSurfaceTest, message_height_used_in_buffer_creation )
{
    using namespace testing;

    geom::Size sz;
    std::shared_ptr<MirBufferPackage> submitted_package;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<1>(&sz),
                        Return(mock_buffer_factory->emptybuffer)));

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_EQ(sz.height, (unsigned int) mock_server_tool->height_sent);
}

TEST_F(MirClientSurfaceTest, message_pf_used_in_buffer_creation )
{
    using namespace testing;

    geom::PixelFormat pf;
    std::shared_ptr<MirBufferPackage> submitted_package;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1)
        .WillOnce(DoAll(SaveArg<2>(&pf),
                        Return(mock_buffer_factory->emptybuffer)));

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);

    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_EQ(pf, geom::PixelFormat::abgr_8888);
}

TEST_F(MirClientSurfaceTest, get_buffer_returns_last_received_buffer_package)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1);

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_THAT(*surface->get_current_buffer_package(), BufferPackageMatches(mock_server_tool->server_package));

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1);
    auto buffer_wait_handle = surface->next_buffer(&empty_surface_callback, nullptr);
    buffer_wait_handle->wait_for_result();

    EXPECT_THAT(*surface->get_current_buffer_package(), BufferPackageMatches(mock_server_tool->server_package));
}

TEST_F(MirClientSurfaceTest, default_surface_type)
{
    using namespace testing;
    using namespace mir::protobuf;

    EXPECT_CALL(*mock_buffer_factory, create_buffer(_,_,_))
        .Times(1);

    auto surface = std::make_shared<MirSurface> (connection.get(),
                                                 *client_comm_channel,
                                                 logger,
                                                 mock_buffer_factory,
                                                 params,
                                                 &empty_callback,
                                                 nullptr);
    surface->get_create_wait_handle()->wait_for_result();

    EXPECT_EQ(mir_surface_type_normal,
              surface->attrib(mir_surface_attrib_type));
}
