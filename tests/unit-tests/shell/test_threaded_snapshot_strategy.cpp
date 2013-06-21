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

#include "mir/shell/threaded_snapshot_strategy.h"
#include "mir/shell/buffer_pixels.h"
#include "mir/shell/surface_buffer_access.h"
#include "mir/compositor/buffer.h"

#include "mir_test_doubles/stub_buffer.h"
#include "mir_test/fake_shared.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <atomic>

namespace mc = mir::compositor;
namespace msh = mir::shell;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;

struct StubSurfaceBufferAccess : public msh::SurfaceBufferAccess
{
    void with_most_recent_buffer_do(
        std::function<void(mc::Buffer&)> const& exec)
    {
        exec(buffer);
    }

    mtd::StubBuffer buffer;
};

struct MockBufferPixels : public msh::BufferPixels
{
    MOCK_METHOD1(extract_from, void(mc::Buffer& buffer));
    MOCK_METHOD0(as_argb_8888, msh::BufferPixelsData());
};

TEST(ThreadedSnapshotStrategyTest, takes_snapshot)
{
    using namespace testing;

    msh::BufferPixelsData const bpd{
        geom::Size{geom::Width{10}, geom::Height{11}},
        geom::Stride{123},
        reinterpret_cast<void*>(0xabcd)};

    MockBufferPixels buffer_pixels;
    StubSurfaceBufferAccess buffer_access;

    EXPECT_CALL(buffer_pixels, extract_from(Ref(buffer_access.buffer)));
    EXPECT_CALL(buffer_pixels, as_argb_8888())
        .WillOnce(Return(bpd));

    msh::ThreadedSnapshotStrategy strategy{mt::fake_shared(buffer_pixels)};

    std::atomic<bool> snapshot_taken{false};

    msh::Snapshot snapshot;

    strategy.take_snapshot_of(
        mt::fake_shared(buffer_access),
        [&](msh::Snapshot const& s)
        { 
            snapshot = s;
            snapshot_taken = true; 
        });

    while (!snapshot_taken)
        std::this_thread::sleep_for(std::chrono::milliseconds{1});

    EXPECT_EQ(bpd.size,   snapshot.size);
    EXPECT_EQ(bpd.stride, snapshot.stride);
    EXPECT_EQ(bpd.pixels, snapshot.pixels);
}
