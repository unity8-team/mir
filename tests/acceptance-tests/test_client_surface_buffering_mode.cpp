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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/debug/surface.h"

#include "mir_test_framework/connected_client_headless_server.h"
#include "mir_test_framework/stub_server_platform_factory.h"

#include <unordered_set>
#include <chrono>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mtf = mir_test_framework;

using namespace std::literals::chrono_literals;

namespace
{

struct WithDefaultSurfaceBuffers : mtf::ConnectedClientHeadlessServer,
                                   ::testing::WithParamInterface<int>
{
    WithDefaultSurfaceBuffers()
    {
        add_to_environment("MIR_SERVER_NBUFFERS", std::to_string(GetParam()).c_str());
        // In triple buffering mode the server initially allocates only two buffers,
        // and the third on demand. We use a non-zero vsync interval so that the client
        // can swap buffers faster than the server can process them, which forces the
        // server to allocate all surface buffers.
        mtf::set_next_vsync_interval(5ms);
    }

    template<typename T> using UPtr = std::unique_ptr<T,void(*)(T*)>;

    UPtr<MirSurface> create_surface_with_buffering_mode(MirBufferingMode mode)
    {
        auto const spec = UPtr<MirSurfaceSpec>{
            mir_connection_create_spec_for_normal_surface(
                connection, 640, 480, mir_pixel_format_argb_8888),
            mir_surface_spec_release};

        mir_surface_spec_set_buffering_mode(spec.get(), mode);

        return UPtr<MirSurface>{
            mir_surface_create_sync(spec.get()), mir_surface_release_sync};
    }

    UPtr<MirSurface> create_surface_without_buffering_mode()
    {
        auto const spec = UPtr<MirSurfaceSpec>{
            mir_connection_create_spec_for_normal_surface(
                connection, 640, 480, mir_pixel_format_argb_8888),
            mir_surface_spec_release};

        return UPtr<MirSurface>{
            mir_surface_create_sync(spec.get()), mir_surface_release_sync};
    }

    unsigned int number_of_buffers_for(MirSurface* surface)
    {
        std::unordered_set<uint32_t> buffers;

        for (int i = 0; i != 10; ++i)
        {
            mir_buffer_stream_swap_buffers_sync(
                mir_surface_get_buffer_stream(surface));
            buffers.insert(
                mir_debug_surface_current_buffer_id(surface));
        }

        return buffers.size();
    }
};

}

TEST_P(WithDefaultSurfaceBuffers, allocates_two_buffers_when_client_requests_double_buffering)
{
    using namespace testing;

    auto const surface = create_surface_with_buffering_mode(mir_buffering_mode_double);

    EXPECT_THAT(number_of_buffers_for(surface.get()), Eq(2));
}

TEST_P(WithDefaultSurfaceBuffers, allocates_three_buffers_when_client_requests_triple_buffering)
{
    using namespace testing;

    auto const surface = create_surface_with_buffering_mode(mir_buffering_mode_triple);

    EXPECT_THAT(number_of_buffers_for(surface.get()), Eq(3));
}

TEST_P(WithDefaultSurfaceBuffers, allocates_default_buffers_when_client_requests_default_buffering)
{
    using namespace testing;

    auto const surface = create_surface_with_buffering_mode(mir_buffering_mode_default);

    EXPECT_THAT(number_of_buffers_for(surface.get()), Eq(GetParam()));
}

TEST_P(WithDefaultSurfaceBuffers, allocates_default_buffers_when_client_does_not_specify_buffering)
{
    using namespace testing;

    auto const surface = create_surface_without_buffering_mode();

    EXPECT_THAT(number_of_buffers_for(surface.get()), Eq(GetParam()));
}

INSTANTIATE_TEST_CASE_P(MirServer,
                        WithDefaultSurfaceBuffers,
                        ::testing::Values(2,3));
