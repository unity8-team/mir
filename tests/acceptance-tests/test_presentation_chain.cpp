/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#include "mir_toolkit/mir_render_surface.h"
#include "mir_toolkit/mir_presentation_chain.h"
#include "mir_toolkit/mir_buffer.h"

#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/mir_extension_core.h"
#include "mir_toolkit/extensions/fenced_buffers.h"
#include "mir_test_framework/connected_client_headless_server.h"
#include "mir/geometry/size.h"
#include "mir/fd.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace mtf = mir_test_framework;
namespace mt = mir::test;
namespace mc = mir::compositor;
namespace geom = mir::geometry;
using namespace testing;
using namespace std::chrono_literals;

namespace
{
struct Chain
{
    Chain(Chain const&) = delete;
    Chain& operator=(Chain const&) = delete;

    Chain(MirConnection* connection) :
        rs(mir_connection_create_render_surface_sync(connection, 0, 0)),
        chain(mir_render_surface_get_presentation_chain(rs))
    {
    }

    MirRenderSurface* content()
    {
        return rs;
    }

    operator MirPresentationChain*()
    {
        return chain;
    }

    ~Chain()
    {
        mir_render_surface_release(rs);
    }
private:
    MirRenderSurface* rs;
    MirPresentationChain* chain;
};

class SurfaceWithChain
{
public:
    SurfaceWithChain(SurfaceWithChain const&) = delete;
    SurfaceWithChain& operator=(SurfaceWithChain const&) = delete;

    operator MirWindow*()
    {
        return window;
    }

    Chain& chain()
    {
        return chain_;
    }

    ~SurfaceWithChain()
    {
        mir_window_release_sync(window);
    }
protected:
    SurfaceWithChain(MirConnection* connection, std::function<MirWindow*(Chain&)> const& fn) :
        chain_(connection),
        window(fn(chain_))
    {
    }
private:
    Chain chain_;
    MirWindow* window;
};

struct SurfaceWithChainFromStart : SurfaceWithChain
{
    SurfaceWithChainFromStart(SurfaceWithChainFromStart const&) = delete;
    SurfaceWithChainFromStart& operator=(SurfaceWithChainFromStart const&) = delete;

    SurfaceWithChainFromStart(MirConnection* connection, geom::Size size, MirPixelFormat pf) :
        SurfaceWithChain(connection,
        std::bind(&SurfaceWithChainFromStart::create_surface, this,
            std::placeholders::_1, connection, size, pf))
    {
    }
private:
    MirWindow* create_surface(Chain& chain, MirConnection* connection, geom::Size size, MirPixelFormat pf)
    {
        auto spec = mir_create_normal_window_spec(
            connection, size.width.as_int(), size.height.as_int());
        mir_window_spec_set_pixel_format(spec, pf);
        mir_surface_spec_add_render_surface(
            spec, chain.content(), size.width.as_int(), size.height.as_int(), 0, 0);
        auto window = mir_create_window_sync(spec);
        mir_window_spec_release(spec);
        return window;
    }
};

struct SurfaceWithChainFromReassociation : SurfaceWithChain
{
    SurfaceWithChainFromReassociation(SurfaceWithChainFromReassociation const&) = delete;
    SurfaceWithChainFromReassociation& operator=(SurfaceWithChainFromReassociation const&) = delete;
    SurfaceWithChainFromReassociation(MirConnection* connection, geom::Size size, MirPixelFormat pf) :
        SurfaceWithChain(connection,
        std::bind(&SurfaceWithChainFromReassociation::create_surface, this,
            std::placeholders::_1, connection, size, pf))
    {
    }
private:
    MirWindow* create_surface(Chain& chain, MirConnection* connection, geom::Size size, MirPixelFormat pf)
    {
        MirWindowSpec* spec = mir_create_normal_window_spec(
            connection, size.width.as_int(), size.height.as_int());
        mir_window_spec_set_pixel_format(spec, pf);
        auto window = mir_create_window_sync(spec);
        mir_window_spec_release(spec);
        spec = mir_create_window_spec(connection);
        mir_surface_spec_add_render_surface(
            spec, chain.content(), size.width.as_int(), size.height.as_int(), 0, 0);
        mir_window_apply_spec(window, spec);
        mir_window_spec_release(spec);
        return window;
    }
};

struct PresentationChain : mtf::ConnectedClientHeadlessServer
{
    geom::Size const size {100, 20};
    MirPixelFormat const pf = mir_pixel_format_abgr_8888;
};

struct MirBufferSync
{
    MirBufferSync() {}
    MirBufferSync(MirBuffer* buffer) :
        buffer_(buffer)
    {
    }

    void buffer_available(MirBuffer* b)
    {
        std::unique_lock<std::mutex> lk(mutex);
        callback_count++;
        buffer_ = b;
        available = true;
        cv.notify_all();
    }

    bool wait_for_buffer(std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lk(mutex);
        return cv.wait_for(lk, timeout, [this] { return available; });
    }

    void unavailable()
    {
        std::unique_lock<std::mutex> lk(mutex);
        available = false;
    }

    MirBuffer* buffer()
    {
        return buffer_;
    }
private:
    std::mutex mutex;
    std::condition_variable cv;
    MirBuffer* buffer_ = nullptr;
    bool available = false;
    unsigned int callback_count = 0;
};

void ignore_callback(MirBuffer*, void*)
{
}

void buffer_callback(MirBuffer* buffer, void* context)
{
    auto sync = reinterpret_cast<MirBufferSync*>(context);
    sync->buffer_available(buffer);
}

}

TEST_F(PresentationChain, allocation_calls_callback)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    MirBufferSync context;
    mir_connection_allocate_buffer(
        connection,
        size.width.as_int(), size.height.as_int(), pf,
        buffer_callback, &context);

    EXPECT_TRUE(context.wait_for_buffer(10s));
    EXPECT_THAT(context.buffer(), Ne(nullptr));    
}

TEST_F(PresentationChain, can_access_platform_message_representing_buffer)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    MirBufferSync context;
    mir_connection_allocate_buffer(
        connection,
        size.width.as_int(), size.height.as_int(), pf,
        buffer_callback, &context);

    EXPECT_TRUE(context.wait_for_buffer(10s));
    auto buffer = context.buffer();
    EXPECT_THAT(context.buffer(), Ne(nullptr));

    auto message = mir_buffer_get_buffer_package(buffer);
    ASSERT_THAT(message, Ne(nullptr));
    EXPECT_THAT(message->data_items, Ge(1));
    EXPECT_THAT(message->fd_items, Ge(1));
    EXPECT_THAT(message->width, Eq(size.width.as_int()));
    EXPECT_THAT(message->height, Eq(size.height.as_int()));
}

TEST_F(PresentationChain, has_native_fence)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    auto ext = mir_extension_fenced_buffers_v1(connection);
    ASSERT_THAT(ext, Ne(nullptr));
    ASSERT_THAT(ext->get_fence, Ne(nullptr));

    MirBufferSync context;
    mir_connection_allocate_buffer(
        connection,
        size.width.as_int(), size.height.as_int(), pf,
        buffer_callback, &context);

    EXPECT_TRUE(context.wait_for_buffer(10s));
    auto buffer = context.buffer();
    EXPECT_THAT(context.buffer(), Ne(nullptr));

    //the native type for the stub platform is nullptr
    EXPECT_THAT(ext->get_fence(buffer), Eq(mir::Fd::invalid));
}

TEST_F(PresentationChain, can_map_for_cpu_render)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    MirGraphicsRegion region;
    MirBufferLayout region_layout = mir_buffer_layout_unknown;
    MirBufferSync context;
    mir_connection_allocate_buffer(
        connection,
        size.width.as_int(), size.height.as_int(), pf,
        buffer_callback, &context);

    EXPECT_TRUE(context.wait_for_buffer(10s));
    auto buffer = context.buffer();
    EXPECT_THAT(context.buffer(), Ne(nullptr));
    mir_buffer_map(buffer, &region, &region_layout);
    //cast to int so gtest doesn't try to print a char* that isn't a string
    EXPECT_THAT(reinterpret_cast<int*>(region.vaddr), Ne(nullptr));
    EXPECT_THAT(region.width, Eq(size.width.as_int()));
    EXPECT_THAT(region.height, Eq(size.height.as_int()));
    EXPECT_THAT(region.stride, Eq(size.width.as_int() * MIR_BYTES_PER_PIXEL(pf)));
    EXPECT_THAT(region.pixel_format, Eq(pf));
    EXPECT_THAT(region_layout, Eq(mir_buffer_layout_linear));
    mir_buffer_unmap(buffer);
}

TEST_F(PresentationChain, submission_will_eventually_call_callback)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    auto const num_buffers = 2u;
    std::array<MirBufferSync, num_buffers> contexts;
    auto num_iterations = 50u;
    for(auto& context : contexts)
    {
        mir_connection_allocate_buffer(
            connection,
            size.width.as_int(), size.height.as_int(), pf,
            buffer_callback, &context);
        ASSERT_TRUE(context.wait_for_buffer(10s));
        ASSERT_THAT(context.buffer(), Ne(nullptr));    
    }

    for(auto i = 0u; i < num_iterations; i++)
    {
        mir_presentation_chain_submit_buffer(
            window.chain(), contexts[i % num_buffers].buffer(),
            buffer_callback, &contexts[i % num_buffers]);
        contexts[i % num_buffers].unavailable();
        if (i != 0)
            ASSERT_TRUE(contexts[(i-1) % num_buffers].wait_for_buffer(10s)) << "iteration " << i;
    }

    for (auto& context : contexts)
        mir_buffer_release(context.buffer());
}

TEST_F(PresentationChain, buffers_can_be_destroyed_before_theyre_returned)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    MirBufferSync context;
    auto buffer = mir_connection_allocate_buffer_sync(
        connection, size.width.as_int(), size.height.as_int(), pf);

    mir_presentation_chain_submit_buffer(window.chain(), buffer, ignore_callback, nullptr);
    mir_buffer_release(buffer);
}

TEST_F(PresentationChain, buffers_can_be_flushed)
{
    SurfaceWithChainFromStart window(connection, size, pf);

    auto buffer = mir_connection_allocate_buffer_sync(
        connection, size.width.as_int(), size.height.as_int(), pf);
    mir_buffer_unmap(buffer);
}

TEST_F(PresentationChain, destroying_a_chain_will_return_buffers_associated_with_chain)
{
    auto rs_chain = mir_connection_create_render_surface_sync(connection, 1, 1);
    auto chain = mir_render_surface_get_presentation_chain(rs_chain);
    auto rs_stream = mir_connection_create_render_surface_sync(connection, 1, 1);
    auto stream = mir_render_surface_get_buffer_stream(rs_stream, 25, 12, mir_pixel_format_abgr_8888);
    ASSERT_TRUE(mir_presentation_chain_is_valid(chain));
    ASSERT_TRUE(mir_render_surface_is_valid(rs_chain));
    ASSERT_TRUE(mir_render_surface_is_valid(rs_stream));

    auto spec = mir_create_normal_window_spec(
        connection, size.width.as_int(), size.height.as_int());
    mir_window_spec_set_pixel_format(spec, pf);
    auto window = mir_create_window_sync(spec);
    mir_window_spec_release(spec);

    spec = mir_create_window_spec(connection);
    mir_surface_spec_add_render_surface(
        spec, rs_chain, size.width.as_int(), size.height.as_int(), 0, 0);
    mir_window_apply_spec(window, spec);
    mir_window_spec_release(spec);

    auto buffer = mir_connection_allocate_buffer_sync(
        connection, size.width.as_int(), size.height.as_int(), pf);

    MirBufferSync context(buffer);
    context.unavailable();
    mir_presentation_chain_submit_buffer(chain, context.buffer(), buffer_callback, &context);

    spec = mir_create_window_spec(connection);
    mir_surface_spec_add_render_surface(spec, rs_stream, size.width.as_int(), size.height.as_int(), 0, 0);
    mir_window_apply_spec(window, spec);
    mir_window_spec_release(spec);
    mir_render_surface_release(rs_chain);
    mir_buffer_stream_swap_buffers_sync(stream);

    ASSERT_TRUE(context.wait_for_buffer(10s));

    mir_render_surface_release(rs_stream);
    mir_window_release_sync(window);
}

TEST_F(PresentationChain, can_access_basic_buffer_properties)
{
    geom::Width width { 32 };
    geom::Height height { 33 };
    auto format = mir_pixel_format_abgr_8888;

    SurfaceWithChainFromStart window(connection, size, pf);
    auto buffer = mir_connection_allocate_buffer_sync(
        connection, width.as_int(), height.as_int(), format);
    EXPECT_THAT(mir_buffer_get_width(buffer), Eq(width.as_uint32_t()));
    EXPECT_THAT(mir_buffer_get_height(buffer), Eq(height.as_uint32_t()));
    EXPECT_THAT(mir_buffer_get_pixel_format(buffer), Eq(format));
}

TEST_F(PresentationChain, can_check_valid_buffers)
{
    auto buffer = mir_connection_allocate_buffer_sync(
        connection, size.width.as_int(), size.height.as_int(), pf);
    ASSERT_THAT(buffer, Ne(nullptr));
    EXPECT_TRUE(mir_buffer_is_valid(buffer));
    EXPECT_THAT(mir_buffer_get_error_message(buffer), StrEq(""));
}

TEST_F(PresentationChain, can_check_invalid_buffers)
{
    auto buffer = mir_connection_allocate_buffer_sync(connection, 0, 0, pf);
    ASSERT_THAT(buffer, Ne(nullptr));
    EXPECT_FALSE(mir_buffer_is_valid(buffer));
    EXPECT_THAT(mir_buffer_get_error_message(buffer), Not(StrEq("")));
    mir_buffer_release(buffer);
}
