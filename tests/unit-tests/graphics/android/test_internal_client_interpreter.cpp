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

#include "src/platform/graphics/android/internal_client_window.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_interpreter_resource_cache.h"
#include "mir_test_doubles/mock_fence.h"
#include "mir/graphics/internal_surface.h"
#include "mir_test_doubles/stub_android_native_buffer.h"

#include "gmock_set_arg.h"
#include <gtest/gtest.h>
#include <stdexcept>

namespace mg=mir::graphics;
namespace mtd=mir::test::doubles;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

namespace
{
class MockInternalSurface : public mg::InternalSurface
{
public:
    MOCK_CONST_METHOD0(size, geom::Size());
    MOCK_CONST_METHOD0(pixel_format, MirPixelFormat());
    MOCK_METHOD1(swap_buffers, void(mg::Buffer*&));
};

struct InternalClientWindow : public ::testing::Test
{
    void SetUp()
    {
        using namespace testing;
        sz = geom::Size{4, 23};
        mock_surface = std::make_shared<NiceMock<MockInternalSurface>>();
        stub_native_buffer = std::make_shared<mtd::StubAndroidNativeBuffer>(); 

        ON_CALL(*mock_surface, swap_buffers(_))
            .WillByDefault(SetArg<0>(&mock_buffer));
        ON_CALL(*mock_surface, pixel_format())
            .WillByDefault(Return(mir_pixel_format_abgr_8888));
        ON_CALL(mock_buffer, native_buffer_handle())
            .WillByDefault(Return(stub_native_buffer));
    }

    std::shared_ptr<mg::NativeBuffer> stub_native_buffer;
    std::shared_ptr<MockInternalSurface> mock_surface;
    mtd::MockBuffer mock_buffer;
    geom::Size sz;
};

}

TEST_F(InternalClientWindow, ReturnsBufferFromSurfaceWhenRequested)
{
    using namespace testing;
    EXPECT_CALL(*mock_surface, swap_buffers(_))
        .Times(1);
    EXPECT_CALL(mock_buffer, native_buffer_handle())
        .Times(1);

    mga::InternalClientWindow interpreter(mock_surface);
    auto test_buffer = interpreter.driver_requests_buffer();
    ASSERT_NE(nullptr, test_buffer);
    EXPECT_EQ(stub_native_buffer.get(), test_buffer);
}

TEST_F(InternalClientWindow, ReportsCorrectSizeAndConcreteType)
{
    using namespace testing;
    EXPECT_CALL(*mock_surface, size())
        .Times(2)
        .WillOnce(Return(sz))
        .WillOnce(Return(sz));
    mga::InternalClientWindow interpreter(mock_surface);

    unsigned int rc_width = interpreter.driver_requests_info(NATIVE_WINDOW_WIDTH);
    unsigned int rc_height = interpreter.driver_requests_info(NATIVE_WINDOW_HEIGHT);
    unsigned int rc_concrete_type = interpreter.driver_requests_info(NATIVE_WINDOW_CONCRETE_TYPE);

    EXPECT_EQ(NATIVE_WINDOW_SURFACE, rc_concrete_type);
    EXPECT_EQ(sz.width.as_uint32_t(), rc_width);
    EXPECT_EQ(sz.height.as_uint32_t(), rc_height);
}

TEST_F(InternalClientWindow, ReportsDriverDefaultFormat)
{
    using namespace testing;
    EXPECT_CALL(*mock_surface, pixel_format())
        .Times(1);

    mga::InternalClientWindow interpreter(mock_surface);

    auto rc_format = interpreter.driver_requests_info(NATIVE_WINDOW_FORMAT);
    EXPECT_EQ(HAL_PIXEL_FORMAT_RGBA_8888, rc_format);
}

TEST_F(InternalClientWindow, ReportsDifferentFormatAfterDriverSetsFormat)
{
    using namespace testing;
    EXPECT_CALL(*mock_surface, pixel_format())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(mir_pixel_format_abgr_8888));
    mga::InternalClientWindow interpreter(mock_surface);

    interpreter.dispatch_driver_request_format(HAL_PIXEL_FORMAT_RGBA_8888);
    auto rc_format = interpreter.driver_requests_info(NATIVE_WINDOW_FORMAT);
    EXPECT_EQ(HAL_PIXEL_FORMAT_RGBA_8888, rc_format);
}
