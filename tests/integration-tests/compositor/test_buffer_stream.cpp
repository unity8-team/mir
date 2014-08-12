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

#include "src/server/compositor/buffer_stream_surfaces.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/time/timer.h"
#include "src/server/compositor/buffer_queue.h"
#include "src/server/compositor/timeout_frame_dropping_policy_factory.h"

#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/stub_buffer_allocator.h"
#include "mir_test_doubles/mock_timer.h"
#include "mir_test/signal.h"

#include <gmock/gmock.h>

#include <condition_variable>
#include <thread>
#include <chrono>
#include <atomic>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mtd = mir::test::doubles;
namespace mt =mir::test;
namespace geom = mir::geometry;

using namespace ::testing;

namespace
{

struct BufferStreamSurfaces : mc::BufferStreamSurfaces
{
    using mc::BufferStreamSurfaces::BufferStreamSurfaces;

    // Convenient function to allow tests to be written in linear style
    void swap_client_buffers_blocking(mg::Buffer*& buffer)
    {
        swap_client_buffers_cancellable(buffer, std::make_shared<mt::Signal>());
    }

    void swap_client_buffers_cancellable(mg::Buffer*& buffer, std::shared_ptr<mt::Signal> const& signal)
    {
        if (buffer)
            release_client_buffer(buffer);

        acquire_client_buffer(
            [signal, &buffer](mg::Buffer* new_buffer)
             {
                buffer = new_buffer;
                signal->raise();
             });

        signal->wait();
    }
};

struct BufferStreamTest : public ::testing::Test
{
    BufferStreamTest()
        : clock{std::make_shared<mt::FakeClock>()},
          timer{std::make_shared<mtd::FakeTimer>(clock)},
          frame_drop_timeout{1000},
          nbuffers{3},
          buffer_stream{create_bundle()}
    {
    }

    int buffers_free_for_client() const
    {
        return buffer_queue->buffers_free_for_client();
    }

    std::shared_ptr<mc::BufferBundle> create_bundle()
    {
        auto allocator = std::make_shared<mtd::StubBufferAllocator>();
        mg::BufferProperties properties{geom::Size{380, 210},
                                        mir_pixel_format_abgr_8888,
                                        mg::BufferUsage::hardware};
        mc::TimeoutFrameDroppingPolicyFactory policy_factory{timer,
                                                             frame_drop_timeout};

        buffer_queue = std::make_shared<mc::BufferQueue>(nbuffers,
                                                         allocator,
                                                         properties,
                                                         policy_factory);

        return buffer_queue;
    }

    std::shared_ptr<mt::FakeClock> clock;
    std::shared_ptr<mtd::FakeTimer> timer;
    std::chrono::milliseconds const frame_drop_timeout;
    const int nbuffers;
    std::shared_ptr<mc::BufferQueue> buffer_queue;
    BufferStreamSurfaces buffer_stream;
};

}

TEST_F(BufferStreamTest, gives_same_back_buffer_until_more_available)
{
    ASSERT_THAT(buffers_free_for_client(), Ge(2)); // else we will hang

    mg::Buffer* client1{nullptr};
    buffer_stream.swap_client_buffers_blocking(client1);
    auto client1_id = client1->id();
    buffer_stream.swap_client_buffers_blocking(client1);

    auto comp1 = buffer_stream.lock_compositor_buffer(nullptr);
    auto comp2 = buffer_stream.lock_compositor_buffer(nullptr);

    EXPECT_EQ(comp1->id(), comp2->id());
    EXPECT_EQ(comp1->id(), client1_id);

    comp1.reset();

    buffer_stream.swap_client_buffers_blocking(client1);
    auto comp3 = buffer_stream.lock_compositor_buffer(nullptr);

    EXPECT_NE(client1_id, comp3->id());

    comp2.reset();
    auto comp3_id = comp3->id();
    comp3.reset();

    auto comp4 = buffer_stream.lock_compositor_buffer(nullptr);
    EXPECT_EQ(comp3_id, comp4->id());
}

TEST_F(BufferStreamTest, gives_all_monitors_the_same_buffer)
{
    mg::Buffer* client_buffer{nullptr};
    int const prefill = buffers_free_for_client();
    for (int i = 0; i < prefill; ++i)
        buffer_stream.swap_client_buffers_blocking(client_buffer);

    auto first_monitor = buffer_stream.lock_compositor_buffer(0);
    auto first_compositor_id = first_monitor->id();
    first_monitor.reset();

    for (int m = 1; m <= 10; m++)
    {
        const void *th = reinterpret_cast<const void*>(m);
        auto monitor = buffer_stream.lock_compositor_buffer(th);
        ASSERT_EQ(first_compositor_id, monitor->id());
    }
}

TEST_F(BufferStreamTest, gives_different_back_buffer_asap)
{
    ASSERT_THAT(buffers_free_for_client(), Ge(2)); // else we will hang

    mg::Buffer* client_buffer{nullptr};
    buffer_stream.swap_client_buffers_blocking(client_buffer);

    ASSERT_THAT(nbuffers, Gt(1));
    buffer_stream.swap_client_buffers_blocking(client_buffer);
    auto comp1 = buffer_stream.lock_compositor_buffer(nullptr);

    buffer_stream.swap_client_buffers_blocking(client_buffer);
    auto comp2 = buffer_stream.lock_compositor_buffer(nullptr);

    EXPECT_NE(comp1->id(), comp2->id());

    comp1.reset();
    comp2.reset();
}

TEST_F(BufferStreamTest, resize_affects_client_buffers_immediately)
{
    ASSERT_THAT(buffers_free_for_client(), Ge(2)); // else we will hang

    auto old_size = buffer_stream.stream_size();

    mg::Buffer* client{nullptr};
    buffer_stream.swap_client_buffers_blocking(client);
    EXPECT_EQ(old_size, client->size());

    geom::Size const new_size
    {
        old_size.width.as_int() * 2,
        old_size.height.as_int() * 3
    };
    buffer_stream.resize(new_size);
    EXPECT_EQ(new_size, buffer_stream.stream_size());

    buffer_stream.swap_client_buffers_blocking(client);
    EXPECT_EQ(new_size, client->size());

    buffer_stream.resize(old_size);
    EXPECT_EQ(old_size, buffer_stream.stream_size());

    /* Release a buffer so client can acquire another */
    auto comp = buffer_stream.lock_compositor_buffer(nullptr);
    comp.reset();

    buffer_stream.swap_client_buffers_blocking(client);
    EXPECT_EQ(old_size, client->size());
}

TEST_F(BufferStreamTest, compositor_gets_resized_buffers)
{
    ASSERT_THAT(buffers_free_for_client(), Ge(2)); // else we will hang

    auto old_size = buffer_stream.stream_size();

    mg::Buffer* client{nullptr};
    buffer_stream.swap_client_buffers_blocking(client);

    geom::Size const new_size
    {
        old_size.width.as_int() * 2,
        old_size.height.as_int() * 3
    };
    buffer_stream.resize(new_size);
    EXPECT_EQ(new_size, buffer_stream.stream_size());

    buffer_stream.swap_client_buffers_blocking(client);

    auto comp1 = buffer_stream.lock_compositor_buffer(nullptr);
    EXPECT_EQ(old_size, comp1->size());
    comp1.reset();

    buffer_stream.swap_client_buffers_blocking(client);

    auto comp2 = buffer_stream.lock_compositor_buffer(nullptr);
    EXPECT_EQ(new_size, comp2->size());
    comp2.reset();

    buffer_stream.swap_client_buffers_blocking(client);

    auto comp3 = buffer_stream.lock_compositor_buffer(nullptr);
    EXPECT_EQ(new_size, comp3->size());
    comp3.reset();

    buffer_stream.resize(old_size);
    EXPECT_EQ(old_size, buffer_stream.stream_size());

    // No client frames "drawn" since resize(old_size), so compositor gets new_size
    buffer_stream.swap_client_buffers_blocking(client);
    auto comp4 = buffer_stream.lock_compositor_buffer(nullptr);
    EXPECT_EQ(new_size, comp4->size());
    comp4.reset();

    // Generate a new frame, which should be back to old_size now
    buffer_stream.swap_client_buffers_blocking(client);
    auto comp5 = buffer_stream.lock_compositor_buffer(nullptr);
    EXPECT_EQ(old_size, comp5->size());
    comp5.reset();
}

TEST_F(BufferStreamTest, can_get_partly_released_back_buffer)
{
    ASSERT_THAT(buffers_free_for_client(), Ge(2)); // else we will hang

    mg::Buffer* client{nullptr};
    buffer_stream.swap_client_buffers_blocking(client);
    buffer_stream.swap_client_buffers_blocking(client);

    int a, b, c;
    auto comp1 = buffer_stream.lock_compositor_buffer(&a);
    auto comp2 = buffer_stream.lock_compositor_buffer(&b);

    EXPECT_EQ(comp1->id(), comp2->id());

    comp1.reset();

    auto comp3 = buffer_stream.lock_compositor_buffer(&c);

    EXPECT_EQ(comp2->id(), comp3->id());
}

namespace
{

void client_loop(int nframes, BufferStreamSurfaces& stream)
{
    mg::Buffer* out_buffer{nullptr};
    for (int f = 0; f < nframes; f++)
    {
        stream.swap_client_buffers_blocking(out_buffer);
        ASSERT_NE(nullptr, out_buffer);
        std::this_thread::yield();
    }
}

void compositor_loop(mc::BufferStream &stream,
                     std::atomic<bool> &done)
{
    while (!done.load())
    {
        auto comp1 = stream.lock_compositor_buffer(nullptr);
        ASSERT_NE(nullptr, comp1);

        // Also stress test getting a second compositor buffer before yielding
        auto comp2 = stream.lock_compositor_buffer(nullptr);
        ASSERT_NE(nullptr, comp2);

        std::this_thread::yield();

        comp1.reset();
        comp2.reset();
    }
}

void snapshot_loop(mc::BufferStream &stream,
                   std::atomic<bool> &done)
{
    while (!done.load())
    {
        auto out_region = stream.lock_snapshot_buffer();
        ASSERT_NE(nullptr, out_region);
        std::this_thread::yield();
    }
}

}

TEST_F(BufferStreamTest, stress_test_distinct_buffers)
{
    // More would be good, but armhf takes too long
    const int num_snapshotters{2};
    const int num_frames{200};

    std::atomic<bool> done;
    done = false;

    std::thread client(client_loop,
                       num_frames,
                       std::ref(buffer_stream));

    std::thread compositor(compositor_loop,
                           std::ref(buffer_stream),
                           std::ref(done));

    std::vector<std::shared_ptr<std::thread>> snapshotters;
    for (unsigned int i = 0; i < num_snapshotters; i++)
    {
        snapshotters.push_back(
            std::make_shared<std::thread>(snapshot_loop,
                                          std::ref(buffer_stream),
                                          std::ref(done)));
    }

    client.join();

    done = true;

    buffer_stream.force_requests_to_complete();

    compositor.join();

    for (auto &s : snapshotters)
        s->join();
}

TEST_F(BufferStreamTest, blocked_client_is_released_on_timeout)
{
    using namespace testing;

    mg::Buffer* placeholder{nullptr};

    // Grab all the buffers...
    // TODO: the magic “nbuffers - 1” number should be removed
    for (int i = 0; i < nbuffers - 1; ++i)
        buffer_stream.swap_client_buffers_blocking(placeholder);

    auto swap_completed = std::make_shared<mt::Signal>();
    buffer_stream.acquire_client_buffer([swap_completed](mg::Buffer*) {swap_completed->raise();});

    EXPECT_FALSE(swap_completed->raised());
    clock->advance_time(frame_drop_timeout + std::chrono::milliseconds{1});

    EXPECT_TRUE(swap_completed->wait_for(std::chrono::milliseconds{100}));
}