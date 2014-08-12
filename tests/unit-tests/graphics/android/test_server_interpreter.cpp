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

#include "src/platform/graphics/android/server_render_window.h"

#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_fence.h"
#include "mir_test_doubles/mock_interpreter_resource_cache.h"
#include "mir_test_doubles/mock_framebuffer_bundle.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_android_native_buffer.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <stdexcept>

namespace mt=mir::test;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;
namespace mg=mir::graphics;
namespace mga=mir::graphics::android;

namespace
{
struct ServerRenderWindow : public ::testing::Test
{
    virtual void SetUp()
    {
        using namespace testing;
        mock_buffer1 = std::make_shared<NiceMock<mtd::MockBuffer>>();
        mock_buffer2 = std::make_shared<NiceMock<mtd::MockBuffer>>();
        mock_buffer3 = std::make_shared<NiceMock<mtd::MockBuffer>>();
        mock_fb_bundle = std::make_shared<NiceMock<mtd::MockFBBundle>>();
        mock_cache = std::make_shared<NiceMock<mtd::MockInterpreterResourceCache>>();
        ON_CALL(*mock_fb_bundle, fb_format())
            .WillByDefault(Return(mir_pixel_format_abgr_8888));
    }

    std::shared_ptr<mtd::MockBuffer> mock_buffer1;
    std::shared_ptr<mtd::MockBuffer> mock_buffer2;
    std::shared_ptr<mtd::MockBuffer> mock_buffer3;
    std::shared_ptr<mtd::MockInterpreterResourceCache> mock_cache;
    std::shared_ptr<mtd::MockFBBundle> mock_fb_bundle;
};
}

TEST_F(ServerRenderWindow, returns_buffer_on_request)
{
    using namespace testing;

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    auto stub_buffer = std::make_shared<mtd::StubAndroidNativeBuffer>();

    EXPECT_CALL(*mock_fb_bundle, buffer_for_render())
        .Times(1)
        .WillOnce(Return(mock_buffer1));
    EXPECT_CALL(*mock_buffer1, native_buffer_handle())
        .Times(1)
        .WillOnce(Return(stub_buffer));

    std::shared_ptr<mg::Buffer> tmp = mock_buffer1;
    std::shared_ptr<mg::NativeBuffer> tmp2 = stub_buffer;
    EXPECT_CALL(*mock_cache, store_buffer(tmp, tmp2))
        .Times(1);

    auto rc_buffer = render_window.driver_requests_buffer();
    EXPECT_EQ(stub_buffer.get(), rc_buffer);
}

TEST_F(ServerRenderWindow, updates_fences_and_returns_buffer_on_queue)
{
    using namespace testing;
    int fake_fence = 488;
    auto stub_buffer = std::make_shared<mtd::StubAndroidNativeBuffer>();

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    EXPECT_CALL(*mock_fb_bundle, buffer_for_render())
        .Times(1)
        .WillOnce(Return(mock_buffer1));
    EXPECT_CALL(*mock_buffer1, native_buffer_handle())
        .Times(1)
        .WillOnce(Return(stub_buffer));

    render_window.driver_requests_buffer();
    testing::Mock::VerifyAndClearExpectations(mock_fb_bundle.get());

    std::shared_ptr<mg::Buffer> buf1 = mock_buffer1;
    EXPECT_CALL(*mock_cache, update_native_fence(stub_buffer->anwb(), fake_fence))
        .Times(1);
    EXPECT_CALL(*mock_cache, retrieve_buffer(stub_buffer->anwb()))
        .Times(1)
        .WillOnce(Return(mock_buffer1));

    render_window.driver_returns_buffer(stub_buffer->anwb(), fake_fence);
    testing::Mock::VerifyAndClearExpectations(mock_fb_bundle.get());
}

TEST_F(ServerRenderWindow, returns_format)
{
    using namespace testing;

    EXPECT_CALL(*mock_fb_bundle, fb_format())
        .Times(1)
        .WillOnce(Return(mir_pixel_format_abgr_8888));

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    EXPECT_EQ(HAL_PIXEL_FORMAT_RGBA_8888, render_window.driver_requests_info(NATIVE_WINDOW_FORMAT));
}

TEST_F(ServerRenderWindow, returns_different_format_if_format_changes)
{
    using namespace testing;

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    render_window.dispatch_driver_request_format(HAL_PIXEL_FORMAT_RGBX_8888);
    auto rc_format = render_window.driver_requests_info(NATIVE_WINDOW_FORMAT);
    EXPECT_EQ(HAL_PIXEL_FORMAT_RGBX_8888, rc_format);
}

TEST_F(ServerRenderWindow, returns_sensible_size_values_without_size_having_been_set)
{
    using namespace testing;
    geom::Size test_size{4, 5};
    EXPECT_CALL(*mock_fb_bundle, fb_size())
        .Times(4)
        .WillRepeatedly(Return(test_size));

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    unsigned int rc_width = render_window.driver_requests_info(NATIVE_WINDOW_DEFAULT_WIDTH);
    unsigned int rc_height = render_window.driver_requests_info(NATIVE_WINDOW_DEFAULT_HEIGHT);
    EXPECT_EQ(test_size.width.as_uint32_t(), rc_width);
    EXPECT_EQ(test_size.height.as_uint32_t(), rc_height);

    rc_width = render_window.driver_requests_info(NATIVE_WINDOW_WIDTH);
    rc_height = render_window.driver_requests_info(NATIVE_WINDOW_HEIGHT);
    EXPECT_EQ(test_size.width.as_uint32_t(), rc_width);
    EXPECT_EQ(test_size.height.as_uint32_t(), rc_height);
}

TEST_F(ServerRenderWindow, returns_no_transform_when_asked_for_hint)
{
    using namespace testing;

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    EXPECT_EQ(0, render_window.driver_requests_info(NATIVE_WINDOW_TRANSFORM_HINT));
}

TEST_F(ServerRenderWindow, reports_framebuffer_concrete_type)
{
    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    EXPECT_EQ(NATIVE_WINDOW_FRAMEBUFFER, render_window.driver_requests_info(NATIVE_WINDOW_CONCRETE_TYPE));
}

TEST_F(ServerRenderWindow, throws_on_driver_unknown_inquiry)
{
    using namespace testing;
    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);

    EXPECT_THROW({
        render_window.driver_requests_info(NATIVE_WINDOW_CONSUMER_RUNNING_BEHIND);
    }, std::runtime_error);
}

TEST_F(ServerRenderWindow, services_driver_swapinterval_request)
{
    EXPECT_CALL(*mock_fb_bundle, wait_for_consumed_buffer(false))
        .Times(1);

    mga::ServerRenderWindow render_window(mock_fb_bundle, mock_cache);
    render_window.sync_to_display(false);
}