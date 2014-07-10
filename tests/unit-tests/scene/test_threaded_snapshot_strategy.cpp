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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/server/scene/threaded_snapshot_strategy.h"
#include "src/server/scene/pixel_buffer.h"
#include "mir/scene/surface_buffer_access.h"
#include "mir/graphics/buffer.h"

#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/null_pixel_buffer.h"
#include "mir_test/fake_shared.h"
#include "mir_test/wait_condition.h"
#include "mir_test/current_thread_name.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <atomic>

namespace mg = mir::graphics;
namespace ms = mir::scene;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;

namespace
{

class StubSurfaceBufferAccess : public ms::SurfaceBufferAccess
{
public:
    ~StubSurfaceBufferAccess() noexcept {}

    void with_most_recent_buffer_do(
        std::function<void(mg::Buffer&)> const& exec)
    {
        thread_name = mt::current_thread_name();
        exec(buffer);
    }

    mtd::StubBuffer buffer;
    std::string thread_name;
};

class MockPixelBuffer : public ms::PixelBuffer
{
public:
    ~MockPixelBuffer() noexcept {}

    MOCK_METHOD1(fill_from, void(mg::Buffer& buffer));
    MOCK_METHOD0(as_argb_8888, void const*());
    MOCK_CONST_METHOD0(size, geom::Size());
    MOCK_CONST_METHOD0(stride, geom::Stride());
};

struct ThreadedSnapshotStrategyTest : testing::Test
{
    StubSurfaceBufferAccess buffer_access;
};

}

TEST_F(ThreadedSnapshotStrategyTest, takes_snapshot)
{
    using namespace testing;

    void const* pixels{reinterpret_cast<void*>(0xabcd)};
    geom::Size size{10, 11};
    geom::Stride stride{123};

    MockPixelBuffer pixel_buffer;

    EXPECT_CALL(pixel_buffer, fill_from(Ref(buffer_access.buffer)));
    EXPECT_CALL(pixel_buffer, as_argb_8888())
        .WillOnce(Return(pixels));
    EXPECT_CALL(pixel_buffer, size())
        .WillOnce(Return(size));
    EXPECT_CALL(pixel_buffer, stride())
        .WillOnce(Return(stride));

    ms::ThreadedSnapshotStrategy strategy{mt::fake_shared(pixel_buffer)};

    mt::WaitCondition snapshot_taken;

    ms::Snapshot snapshot;

    strategy.take_snapshot_of(
        mt::fake_shared(buffer_access),
        [&](ms::Snapshot const& s)
        {
            snapshot = s;
            snapshot_taken.wake_up_everyone();
        });

    snapshot_taken.wait_for_at_most_seconds(5);

    EXPECT_EQ(size,   snapshot.size);
    EXPECT_EQ(stride, snapshot.stride);
    EXPECT_EQ(pixels, snapshot.pixels);
}

TEST_F(ThreadedSnapshotStrategyTest, names_snapshot_thread)
{
    using namespace testing;

    mtd::NullPixelBuffer pixel_buffer;

    ms::ThreadedSnapshotStrategy strategy{mt::fake_shared(pixel_buffer)};

    mt::WaitCondition snapshot_taken;

    strategy.take_snapshot_of(
        mt::fake_shared(buffer_access),
        [&](ms::Snapshot const&)
        {
            snapshot_taken.wake_up_everyone();
        });

    snapshot_taken.wait_for_at_most_seconds(5);

    EXPECT_THAT(buffer_access.thread_name, Eq("Mir/Snapshot"));
}
