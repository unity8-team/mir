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

#include "src/server/graphics/android/android_graphic_buffer_allocator.h"
#include "mir_test_doubles/mock_android_hw.h"
#include "mir/graphics/buffer_initializer.h"
#include "mir/compositor/buffer_properties.h"

#include "mir_test_doubles/mock_buffer_initializer.h"

#include <hardware/gralloc.h>
#include <gtest/gtest.h>
#include <algorithm>

namespace mg = mir::graphics;
namespace mga = mir::graphics::android;
namespace geom = mir::geometry;
namespace mc = mir::compositor;
namespace mtd = mir::test::doubles;

struct AndroidGraphicBufferAllocatorTest : public ::testing::Test
{
    AndroidGraphicBufferAllocatorTest()
        : null_buffer_initializer{std::make_shared<mg::NullBufferInitializer>()}
    {
    }

    std::shared_ptr<mg::BufferInitializer> const null_buffer_initializer;
    testing::NiceMock<mtd::HardwareAccessMock> hw_access_mock;
};

TEST_F(AndroidGraphicBufferAllocatorTest, allocator_accesses_gralloc_module)
{
    using namespace testing;

    EXPECT_CALL(hw_access_mock, hw_get_module(StrEq(GRALLOC_HARDWARE_MODULE_ID), _))
        .Times(1);

    mga::AndroidGraphicBufferAllocator allocator(null_buffer_initializer);
}

TEST_F(AndroidGraphicBufferAllocatorTest, alloc_buffer_calls_initializer)
{
    using namespace testing;

    auto buffer_initializer = std::make_shared<mtd::MockBufferInitializer>();

    mc::BufferProperties properties{geom::Size{geom::Width{2}, geom::Height{2}},
                                    geom::PixelFormat::abgr_8888,
                                    mc::BufferUsage::hardware};
    mga::AndroidGraphicBufferAllocator allocator{buffer_initializer};
    EXPECT_CALL(*buffer_initializer, operator_call(_))
        .Times(1);

    allocator.alloc_buffer(properties);
}

TEST_F(AndroidGraphicBufferAllocatorTest, alloc_buffer_platform_calls_initializer)
{
    using namespace testing;

    auto buffer_initializer = std::make_shared<mtd::MockBufferInitializer>();

    mga::AndroidGraphicBufferAllocator allocator{buffer_initializer};
    auto size = geom::Size{geom::Width{2}, geom::Height{2}};
    auto pf = geom::PixelFormat::abgr_8888;

    EXPECT_CALL(*buffer_initializer, operator_call(_))
        .Times(1);

    allocator.alloc_buffer_platform(size, pf, mga::BufferUsage::use_hardware);
}

TEST_F(AndroidGraphicBufferAllocatorTest, buffer_usage_converter)
{
    EXPECT_EQ(mga::BufferUsage::use_hardware,
        mga::AndroidGraphicBufferAllocator::convert_from_compositor_usage(mc::BufferUsage::hardware));
    EXPECT_EQ(mga::BufferUsage::use_software,
        mga::AndroidGraphicBufferAllocator::convert_from_compositor_usage(mc::BufferUsage::software));
}
