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
#include "src/client/client_platform.h"
#include "src/client/client_platform_factory.h"
#include "src/client/mir_surface.h"
#include "src/client/mir_connection.h"
#include "mir/frontend/resource_cache.h"

#include "mir_test/test_protobuf_server.h"
#include "mir_test/stub_server_tool.h"
#include "mir_test/test_protobuf_client.h"
#include "mir_test/gmock_fixes.h"

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

    void create_surface(google::protobuf::RpcController* ,
                 const mir::protobuf::SurfaceParameters* request,
                 mir::protobuf::Surface* response,
                 google::protobuf::Closure* done)
    {
        create_surface_response( response );
        surface_name = request->surface_name();
        done->Run();
    }

    void next_buffer(
        ::google::protobuf::RpcController* /*controller*/,
        ::mir::protobuf::SurfaceId const* /*request*/,
        ::mir::protobuf::Buffer* response,
        ::google::protobuf::Closure* done)
    {
        create_buffer_response( response );
        done->Run();
    }

/* helpers */
    void generate_unique_buffer()
    {
        global_buffer_id++;

        int num_fd = 2, num_data = 8;
        for (auto i=0; i<num_fd; i++)
        {
            server_package.fd[i] = i*3;
        }
        for (auto i=0; i<num_data; i++)
        {
            server_package.data[i] = i*2;
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
    MockBuffer()
    {

    }

    MOCK_METHOD0(secure_for_cpu_write, std::shared_ptr<mcl::MemoryRegion>());
    MOCK_CONST_METHOD0(size, geom::Size());
    MOCK_CONST_METHOD0(stride, geom::Stride());
    MOCK_CONST_METHOD0(pixel_format, geom::PixelFormat());
    MOCK_CONST_METHOD0(get_buffer_package, std::shared_ptr<MirBufferPackage>());
    MOCK_METHOD0(get_native_handle, MirNativeBuffer());
};

struct MockClientDepository : public mcl::ClientBufferDepository
{
    MockClientDepository()
    {
        using namespace testing;

        emptybuffer=std::make_shared<MockBuffer>();
        ON_CALL(*this, access_buffer(_))
            .WillByDefault(Return(emptybuffer));
    }

    void deposit_package(std::shared_ptr<MirBufferPackage> && p, int id,
                         geometry::Size size, geometry::PixelFormat pf)
    {
        deposit_package_rv( p, id, size, pf);
    }

    MOCK_METHOD4(deposit_package_rv,
                     void(std::shared_ptr<MirBufferPackage>, int,
                            geom::Size, geom::PixelFormat));
    MOCK_METHOD1(access_buffer, std::shared_ptr<mcl::ClientBuffer>(int));

    std::shared_ptr<mcl::ClientBuffer> emptybuffer;
};


struct StubClientPlatform : public mcl::ClientPlatform
{
    std::shared_ptr<mcl::ClientBufferDepository> create_platform_depository()
    {
        return std::shared_ptr<mcl::ClientBufferDepository>();
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

        mock_depository = std::make_shared<mt::MockClientDepository>();

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
                                                         nullptr);
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
    std::shared_ptr<mt::MockClientDepository> mock_depository;

    mir::protobuf::Connection response;
    mir::protobuf::ConnectParameters connect_parameters;

    std::shared_ptr<mt::TestProtobufServer> test_server;
    std::shared_ptr<mt::TestProtobufClient> client_tools;
    std::shared_ptr<mt::MockServerPackageGenerator> mock_server_tool;

    CallBack callback;

    std::shared_ptr<mir::protobuf::DisplayServer::Stub> client_comm_channel;
};

void empty_callback(MirSurface*, void*) { }
TEST_F(MirClientSurfaceTest, client_buffer_created_on_surface_creation )
{
    using namespace testing;

    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1);

    auto surface = std::make_shared<MirSurface> (connection.get(), *client_comm_channel, logger, mock_depository, params, &empty_callback, (void*) NULL);

    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();
}

namespace
{
void empty_surface_callback(MirSurface*, void*) {}
}

TEST_F(MirClientSurfaceTest, client_buffer_created_on_next_buffer )
{
    using namespace testing;

    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1);

    auto surface = std::make_shared<MirSurface> (connection.get(), *client_comm_channel, logger, mock_depository, params, &empty_callback, (void*) NULL);

    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1);
    auto buffer_wait_handle = surface->next_buffer(&empty_surface_callback, (void*) NULL);
    buffer_wait_handle->wait_for_result();
}

TEST_F(MirClientSurfaceTest, client_buffer_uses_ipc_message_from_server_on_create )
{
    using namespace testing;

    std::shared_ptr<MirBufferPackage> submitted_package;
    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1)
        .WillOnce(
            SaveArg<0>(&submitted_package));

    auto surface = std::make_shared<MirSurface> (connection.get(), *client_comm_channel, logger, mock_depository, params, &empty_callback, (void*) NULL);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    /* check for same contents */
    ASSERT_EQ(submitted_package->data_items, mock_server_tool->server_package.data_items);
    ASSERT_EQ(submitted_package->fd_items,   mock_server_tool->server_package.fd_items);
    ASSERT_EQ(submitted_package->stride,   mock_server_tool->server_package.stride);
    for(auto i=0; i< submitted_package->data_items; i++)
        EXPECT_EQ(submitted_package->data[i], mock_server_tool->server_package.data[i]);
}

TEST_F(MirClientSurfaceTest, message_width_used_in_buffer_creation )
{
    using namespace testing;

    geom::Size sz;
    std::shared_ptr<MirBufferPackage> submitted_package;

    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1)
        .WillOnce(SaveArg<2>(&sz));

    auto surface = std::make_shared<MirSurface> (connection.get(), *client_comm_channel, logger, mock_depository, params, &empty_callback, (void*) NULL);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_EQ(sz.width.as_uint32_t(), (unsigned int) mock_server_tool->width_sent);
}

TEST_F(MirClientSurfaceTest, message_height_used_in_buffer_creation )
{
    using namespace testing;

    geom::Size sz;
    std::shared_ptr<MirBufferPackage> submitted_package;

    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1)
        .WillOnce(SaveArg<2>(&sz));

    auto surface = std::make_shared<MirSurface> (connection.get(), *client_comm_channel, logger, mock_depository, params, &empty_callback, (void*) NULL);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_EQ(sz.height.as_uint32_t(), (unsigned int) mock_server_tool->height_sent);
}

TEST_F(MirClientSurfaceTest, message_pf_used_in_buffer_creation )
{
    using namespace testing;

    geom::PixelFormat pf;
    std::shared_ptr<MirBufferPackage> submitted_package;

    EXPECT_CALL(*mock_depository, deposit_package_rv(_,_,_,_))
        .Times(1)
        .WillOnce(SaveArg<3>(&pf));

    auto surface = std::make_shared<MirSurface> (connection.get(), *client_comm_channel, logger, mock_depository, params, &empty_callback, (void*) NULL);
    auto wait_handle = surface->get_create_wait_handle();
    wait_handle->wait_for_result();

    EXPECT_EQ(pf, geom::PixelFormat::abgr_8888);
}

