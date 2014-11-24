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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "src/client/aging_buffer.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mcl = mir::client;
namespace geom = mir::geometry;

namespace mir
{
namespace test
{

struct MyAgingBuffer : public mcl::AgingBuffer
{
    std::shared_ptr<mcl::MemoryRegion> secure_for_cpu_write() override
    {
        exit(1);
    }

    geom::Size size() const override
    {
        exit(1);
    }

    geom::Stride stride() const override
    {
        exit(1);
    }

    MirPixelFormat pixel_format() const override
    {
        exit(1);
    }

    std::shared_ptr<mir::graphics::NativeBuffer> native_buffer_handle() const override
    {
        exit(1);
    }
    
    void update_from(MirBufferPackage const&) override
    {
    }

    void fill_update_msg(MirBufferPackage&) override
    {
    }
};

TEST(MirClientAgingBufferTest, BufferAgeStartsAtZero)
{
    using namespace testing;

    MyAgingBuffer buffer;

    EXPECT_EQ(0u, buffer.age());
}

TEST(MirClientAgingBufferTest, BufferAgeSetToOneOnSubmit)
{
    using namespace testing;

    MyAgingBuffer buffer;
    buffer.mark_as_submitted();

    EXPECT_EQ(1u, buffer.age());
}

TEST(MirClientAgingBufferTest, BufferAgeIncreasesOnIncrement)
{
    using namespace testing;

    MyAgingBuffer buffer;
    buffer.mark_as_submitted();

    for (uint32_t age = 2; age < 10; ++age)
    {
        buffer.increment_age();
        EXPECT_EQ(age, buffer.age());
    }
}

TEST(MirClientAgingBufferTest, IncrementingAgeHasNoEffectForUnsubmittedBuffer)
{
    using namespace testing;

    MyAgingBuffer buffer;
    buffer.increment_age();

    EXPECT_EQ(0u, buffer.age());
}

}
}
