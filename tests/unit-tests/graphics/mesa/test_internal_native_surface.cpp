/*
 * Copyright © 2013 Canonical Ltd.
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

#include "src/platform/graphics/mesa/internal_native_surface.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/graphics/internal_surface.h"

#include "mir_toolkit/mesa/native_display.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_frontend_surface.h"
#include "mir_test_doubles/mock_buffer.h"

#include "gmock_set_arg.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{
class MockInternalSurface : public mg::InternalSurface
{
public:
    MOCK_CONST_METHOD0(size, geom::Size());
    MOCK_CONST_METHOD0(pixel_format, MirPixelFormat());
    MOCK_METHOD1(swap_buffers, void(mg::Buffer*&));
};

struct InternalNativeSurface : public testing::Test
{
    void SetUp()
    {
        using namespace ::testing;
        mock_surface = std::make_shared<MockInternalSurface>();
    }

    std::shared_ptr<MockInternalSurface> mock_surface;
};

MATCHER_P(ParametersHaveSize, size, "")
{
    if (static_cast<uint32_t>(arg.width) != size.width.as_uint32_t())
        return false;
    if (static_cast<uint32_t>(arg.height) != size.height.as_uint32_t())
        return false;
    return true;
}
}

TEST_F(InternalNativeSurface, surface_advance_buffer_packaging)
{
    using namespace ::testing;

    mtd::MockBuffer buffer;

    auto test_buffer_package = std::make_shared<MirBufferPackage>();
    test_buffer_package->data_items = 2;
    test_buffer_package->data[0] = 1;
    test_buffer_package->data[1] = 2;
    test_buffer_package->fd_items = 2;
    test_buffer_package->fd[0] = 3;
    test_buffer_package->fd[1] = 4;
    test_buffer_package->stride = 77;

    mgm::InternalNativeSurface native_surface(mock_surface);

    EXPECT_CALL(buffer, native_buffer_handle())
        .WillOnce(Return(test_buffer_package));
    EXPECT_CALL(*mock_surface, swap_buffers(_))
        .Times(1)
        .WillOnce(SetArg<0>(&buffer));

    MirBufferPackage buffer_package;
    memset(&buffer_package, 0, sizeof(MirBufferPackage));
    native_surface.surface_advance_buffer(&native_surface, &buffer_package);

    EXPECT_EQ(test_buffer_package->data_items, buffer_package.data_items);
    EXPECT_EQ(test_buffer_package->data[0], buffer_package.data[0]);
    EXPECT_EQ(test_buffer_package->data[1], buffer_package.data[1]);
    EXPECT_EQ(test_buffer_package->fd_items, buffer_package.fd_items);
    EXPECT_EQ(test_buffer_package->fd[0], buffer_package.fd[0]);
    EXPECT_EQ(test_buffer_package->fd[1], buffer_package.fd[1]);
    EXPECT_EQ(test_buffer_package->stride, buffer_package.stride);
}

TEST_F(InternalNativeSurface, surface_get_parameters)
{
    using namespace ::testing;

    geom::Size const test_surface_size{17, 29};
    MirPixelFormat const test_pixel_format = mir_pixel_format_xrgb_8888;
    EXPECT_CALL(*mock_surface, size())
        .Times(1)
        .WillOnce(Return(test_surface_size));
    EXPECT_CALL(*mock_surface, pixel_format())
        .Times(1)
        .WillOnce(Return(test_pixel_format));

    mgm::InternalNativeSurface native_surface(mock_surface);

    MirSurfaceParameters parameters;
    memset(&parameters, 0, sizeof(MirSurfaceParameters));
    native_surface.surface_get_parameters(&native_surface, &parameters);

    EXPECT_THAT(parameters, ParametersHaveSize(test_surface_size));
    EXPECT_EQ(parameters.pixel_format, mir_pixel_format_xrgb_8888);
    // TODO: What to do about buffer usage besides hardware? ~racarr
    EXPECT_EQ(parameters.buffer_usage, mir_buffer_usage_hardware);
}
