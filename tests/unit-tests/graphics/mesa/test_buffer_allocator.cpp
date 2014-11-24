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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "src/platform/graphics/mesa/platform.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "src/platform/graphics/mesa/buffer_allocator.h"
#include "mir/graphics/buffer_properties.h"

#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"
#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_gl.h"
#include "mir_test_doubles/platform_factory.h"
#include "mir_test_framework/udev_environment.h"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <gbm.h>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

class MesaBufferAllocatorTest  : public ::testing::Test
{
protected:
    virtual void SetUp()
    {
        using namespace testing;

        fake_devices.add_standard_device("standard-drm-devices");

        size = geom::Size{300, 200};
        pf = mir_pixel_format_argb_8888;
        usage = mg::BufferUsage::hardware;
        buffer_properties = mg::BufferProperties{size, pf, usage};

        ON_CALL(mock_gbm, gbm_bo_get_handle(_))
        .WillByDefault(Return(mock_gbm.fake_gbm.bo_handle));

        platform = mtd::create_mesa_platform_with_null_dependencies();
        allocator.reset(new mgm::BufferAllocator(
            platform->gbm.device, mgm::BypassOption::allowed));
    }

    // Defaults
    geom::Size size;
    MirPixelFormat pf;
    mg::BufferUsage usage;
    mg::BufferProperties buffer_properties;

    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;
    ::testing::NiceMock<mtd::MockEGL> mock_egl;
    ::testing::NiceMock<mtd::MockGL> mock_gl;
    std::shared_ptr<mgm::Platform> platform;
    std::unique_ptr<mgm::BufferAllocator> allocator;
    mtf::UdevEnvironment fake_devices;
};

TEST_F(MesaBufferAllocatorTest, AllocatorReturnsNonNullBuffer)
{
    using namespace testing;
    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    EXPECT_TRUE(allocator->alloc_buffer(buffer_properties).get() != NULL);
}

TEST_F(MesaBufferAllocatorTest, LargeHardwareBuffersBypass)
{
    using namespace testing;
    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    const mg::BufferProperties properties(geom::Size{1280, 800},
                                          mir_pixel_format_argb_8888,
                                          mg::BufferUsage::hardware);

    auto buf = allocator->alloc_buffer(properties);
    ASSERT_TRUE(buf.get() != NULL);
    EXPECT_TRUE(buf->can_bypass());
}

TEST_F(MesaBufferAllocatorTest, SmallBuffersDontBypass)
{
    using namespace testing;
    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    const mg::BufferProperties properties(geom::Size{100, 100},
                                          mir_pixel_format_argb_8888,
                                          mg::BufferUsage::hardware);

    auto buf = allocator->alloc_buffer(properties);
    ASSERT_TRUE(buf.get() != NULL);
    EXPECT_FALSE(buf->can_bypass());
}

TEST_F(MesaBufferAllocatorTest, SoftwareBuffersDontBypass)
{
    using namespace testing;

    const mg::BufferProperties properties(geom::Size{1920, 1200},
                                          mir_pixel_format_argb_8888,
                                          mg::BufferUsage::software);

    auto buf = allocator->alloc_buffer(properties);
    ASSERT_TRUE(buf.get() != NULL);
    EXPECT_FALSE(buf->can_bypass());
}

TEST_F(MesaBufferAllocatorTest, BypassDisablesWhenOptionIsDisabled)
{
    using namespace testing;
    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    const mg::BufferProperties properties(geom::Size{1280, 800},
                                          mir_pixel_format_argb_8888,
                                          mg::BufferUsage::hardware);

    mgm::BufferAllocator alloc(
        platform->gbm.device,
        mgm::BypassOption::prohibited);
    auto buf = alloc.alloc_buffer(properties);
    ASSERT_TRUE(buf.get() != NULL);
    EXPECT_FALSE(buf->can_bypass());
}

TEST_F(MesaBufferAllocatorTest, CorrectBufferFormatTranslationArgb_8888)
{
    using namespace testing;

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,GBM_FORMAT_ARGB8888,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    allocator->alloc_buffer(mg::BufferProperties{size, mir_pixel_format_argb_8888, usage});
}

TEST_F(MesaBufferAllocatorTest, CorrectBufferFormatTranslationXrgb_8888)
{
    using namespace testing;

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,GBM_FORMAT_XRGB8888,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    allocator->alloc_buffer(mg::BufferProperties{size, mir_pixel_format_xrgb_8888, usage});
}

MATCHER_P(has_flag_set, flag, "")
{
    return arg & flag;
}

TEST_F(MesaBufferAllocatorTest, CreatesHardwareRenderingBuffer)
{
    using namespace testing;

    mg::BufferProperties properties{size, pf, mg::BufferUsage::hardware};

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,has_flag_set(GBM_BO_USE_RENDERING)));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    allocator->alloc_buffer(properties);
}

TEST_F(MesaBufferAllocatorTest, CreatesSoftwareRenderingBuffer)
{
    using namespace testing;

    mg::BufferProperties properties{size, pf, mg::BufferUsage::software};

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_)).Times(0);

    allocator->alloc_buffer(properties);
}

TEST_F(MesaBufferAllocatorTest, CreatesHardwareRenderingBufferForUndefinedUsage)
{
    using namespace testing;

    mg::BufferProperties properties{size, pf, mg::BufferUsage::undefined};

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,has_flag_set(GBM_BO_USE_RENDERING)));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    allocator->alloc_buffer(properties);
}

TEST_F(MesaBufferAllocatorTest, RequestsCorrectBufferDimensions)
{
    using namespace testing;

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,size.width.as_uint32_t(),size.height.as_uint32_t(),_,_));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_));

    allocator->alloc_buffer(buffer_properties);
}

TEST_F(MesaBufferAllocatorTest, CorrectBufferHandleIsDestroyed)
{
    using namespace testing;
    gbm_bo* bo{reinterpret_cast<gbm_bo*>(0xabcd)};

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_))
    .WillOnce(Return(bo));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(bo));

    allocator->alloc_buffer(buffer_properties);
}

TEST_F(MesaBufferAllocatorTest, ThrowsOnBufferCreationFailure)
{
    using namespace testing;

    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_))
        .WillOnce(Return(reinterpret_cast<gbm_bo*>(0)));
    EXPECT_CALL(mock_gbm, gbm_bo_destroy(_))
        .Times(0);

    EXPECT_THROW({
        allocator->alloc_buffer(buffer_properties);
    }, std::runtime_error);
}

TEST_F(MesaBufferAllocatorTest, SupportedPixelFormatsContainCommonFormats)
{
    auto supported_pixel_formats = allocator->supported_pixel_formats();

    auto argb_8888_count = std::count(supported_pixel_formats.begin(),
                                      supported_pixel_formats.end(),
                                      mir_pixel_format_argb_8888);

    auto xrgb_8888_count = std::count(supported_pixel_formats.begin(),
                                      supported_pixel_formats.end(),
                                      mir_pixel_format_xrgb_8888);

    EXPECT_EQ(1, argb_8888_count);
    EXPECT_EQ(1, xrgb_8888_count);
}

TEST_F(MesaBufferAllocatorTest, SupportedPixelFormatsHaveSaneDefaultInFirstPosition)
{
    auto supported_pixel_formats = allocator->supported_pixel_formats();

    ASSERT_FALSE(supported_pixel_formats.empty());
    EXPECT_EQ(mir_pixel_format_argb_8888, supported_pixel_formats[0]);
}

TEST_F(MesaBufferAllocatorTest, AllocWithUnsupportedPixelFormatThrows)
{
    using namespace testing;

    /* We shouldn't try to create a buffer with an unsupported format */
    EXPECT_CALL(mock_gbm, gbm_bo_create(_,_,_,_,_)).Times(0);

    EXPECT_THROW({
        allocator->alloc_buffer(mg::BufferProperties{size, mir_pixel_format_abgr_8888, usage});
    }, std::runtime_error);
}
