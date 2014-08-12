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

#include "mir/graphics/display.h"
#include "mir/graphics/buffer.h"
#include "mir/frontend/session_authorizer.h"
#include "src/server/frontend/protobuf_ipc_factory.h"
#include "src/server/frontend/resource_cache.h"
#include "src/server/frontend/session_mediator.h"

#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_framework/cross_process_sync.h"
#include "mir_test_doubles/stub_buffer_allocator.h"
#include "mir_test_doubles/null_display.h"
#include "mir_test_doubles/null_event_sink.h"
#include "mir_test_doubles/null_display_changer.h"
#include "mir_test_doubles/stub_display_buffer.h"
#include "mir_test_doubles/null_platform.h"
#include "mir_test/display_config_matchers.h"
#include "mir_test_doubles/stub_display_configuration.h"
#include "mir_test/fake_shared.h"

#include "mir_toolkit/mir_client_library.h"

#include <condition_variable>
#include <thread>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace geom = mir::geometry;
namespace mf = mir::frontend;
namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;
namespace mt = mir::test;

namespace
{

class StubDisplay : public mtd::NullDisplay
{
public:
    StubDisplay()
    {
    }

    void for_each_display_buffer(std::function<void(mg::DisplayBuffer&)> const& f) override
    {
        f(display_buffer);
    }

private:
    mtd::NullDisplayBuffer display_buffer;
};

class StubChanger : public mtd::NullDisplayChanger
{
public:
    std::shared_ptr<mg::DisplayConfiguration> active_configuration() override
    {
        return mt::fake_shared(stub_display_config);
    }

    static mtd::StubDisplayConfig stub_display_config;
private:
    mtd::NullDisplayBuffer display_buffer;
};

mtd::StubDisplayConfig StubChanger::stub_display_config{
    2,
    { mir_pixel_format_xrgb_8888 }
};

char const* const mir_test_socket = mtf::test_socket_file().c_str();

class StubGraphicBufferAllocator : public mtd::StubBufferAllocator
{
public:
    std::vector<MirPixelFormat> supported_pixel_formats() override
    {
        return pixel_formats;
    }

    static std::vector<MirPixelFormat> const pixel_formats;
};

std::vector<MirPixelFormat> const StubGraphicBufferAllocator::pixel_formats{
    mir_pixel_format_argb_8888,
    mir_pixel_format_xbgr_8888,
    mir_pixel_format_bgr_888
};

class StubPlatform : public mtd::NullPlatform
{
public:
    std::shared_ptr<mg::GraphicBufferAllocator> create_buffer_allocator(
            std::shared_ptr<mg::BufferInitializer> const& /*buffer_initializer*/) override
    {
        return std::make_shared<StubGraphicBufferAllocator>();
    }

    std::shared_ptr<mg::Display> create_display(
        std::shared_ptr<mg::DisplayConfigurationPolicy> const&,
        std::shared_ptr<mg::GLProgramFactory> const&,
        std::shared_ptr<mg::GLConfig> const&) override
    {
        if (!display)
            display = std::make_shared<StubDisplay>();
        return display;
    }

    std::shared_ptr<StubDisplay> display;
};

}

using AvailableSurfaceFormatsTest = BespokeDisplayServerTestFixture;

TEST_F(AvailableSurfaceFormatsTest, surface_pixel_formats_reach_client)
{
    struct ServerConfig : TestingServerConfiguration
    {
        std::shared_ptr<mg::Platform> the_graphics_platform() override
        {
            using namespace testing;

            if (!platform)
                platform = std::make_shared<StubPlatform>();

            return platform;
        }

        std::shared_ptr<mf::DisplayChanger> the_frontend_display_changer() override
        {
            if (!changer)
                changer = std::make_shared<StubChanger>();
            return changer;
        }

        std::shared_ptr<StubChanger> changer;
        std::shared_ptr<StubPlatform> platform;
    } server_config;

    launch_server_process(server_config);

    struct Client : TestingClientConfiguration
    {
        void exec()
        {
            MirConnection* connection = mir_connect_sync(mir_test_socket, __PRETTY_FUNCTION__);
            ASSERT_TRUE(mir_connection_is_valid(connection));

            unsigned int const format_storage_size = 4;
            MirPixelFormat formats[format_storage_size];
            unsigned int returned_format_size = 0;
            mir_connection_get_available_surface_formats(connection,
                formats, format_storage_size, &returned_format_size);

            ASSERT_EQ(StubGraphicBufferAllocator::pixel_formats.size(), returned_format_size);
            for (auto i=0u; i < returned_format_size; ++i)
            {
                EXPECT_EQ(StubGraphicBufferAllocator::pixel_formats[i],
                          formats[i]) << "i=" << i;
            }

            mir_connection_release(connection);
        }
    } client_config;

    launch_client_process(client_config);
}