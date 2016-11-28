/*
 * Copyright © 2016 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "src/client/frame_clock.h"
#include <gtest/gtest.h>
#include <unordered_map>

using namespace ::testing;
using namespace ::std::chrono_literals;
using mir::time::PosixTimestamp;
using mir::client::FrameClock;

class FrameClockTest : public Test
{
public:
    FrameClockTest()
    {
        with_fake_time = [this](clockid_t clk){ return fake_time[clk]; };
    }

    void SetUp()
    {
        for (auto& c : {CLOCK_MONOTONIC, CLOCK_REALTIME,
                        CLOCK_MONOTONIC_RAW, CLOCK_REALTIME_COARSE,
                        CLOCK_MONOTONIC_COARSE, CLOCK_BOOTTIME})
            fake_time[c] = PosixTimestamp(c, c * c * 1234567ns);
        int const hz = 60;
        one_frame = std::chrono::nanoseconds(1000000000L/hz);
    }

    void fake_sleep_for(std::chrono::nanoseconds ns)
    {
        for (auto& f : fake_time)
            f.second.nanoseconds += ns;
    }

    void fake_sleep_until(PosixTimestamp t)
    {
        auto& now = fake_time[t.clock_id];
        if (t > now)
        {
            auto delta = t.nanoseconds - now.nanoseconds;
            fake_sleep_for(delta);
        }
    }

protected:
    FrameClock::GetCurrentTime with_fake_time;
    std::unordered_map<clockid_t,PosixTimestamp> fake_time;
    std::chrono::nanoseconds one_frame;
};

TEST_F(FrameClockTest, unthrottled_without_a_period)
{
    FrameClock clock(with_fake_time);

    PosixTimestamp a;
    auto b = clock.next_frame_after(a);
    EXPECT_EQ(a, b);
    auto c = clock.next_frame_after(b);
    EXPECT_EQ(b, c);
}

TEST_F(FrameClockTest, first_frame_is_within_one_frame)
{
    auto& now = fake_time[CLOCK_MONOTONIC];
    auto a = now;
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    PosixTimestamp b;
    auto c = clock.next_frame_after(b);
    EXPECT_GE(c, a);
    EXPECT_LE(c-a, one_frame);
}

TEST_F(FrameClockTest, interval_is_perfectly_smooth)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    PosixTimestamp a;
    auto b = clock.next_frame_after(a);

    fake_sleep_until(b);
    fake_sleep_for(one_frame/13);  // short render time

    auto c = clock.next_frame_after(b);
    EXPECT_EQ(one_frame, c - b);

    fake_sleep_until(c);
    fake_sleep_for(one_frame/7);  // short render time

    auto d = clock.next_frame_after(c);
    EXPECT_EQ(one_frame, d - c);

    fake_sleep_until(d);
    fake_sleep_for(one_frame/5);  // short render time

    auto e = clock.next_frame_after(d);
    EXPECT_EQ(one_frame, e - d);
}

TEST_F(FrameClockTest, long_render_time_is_recoverable_without_decimation)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    auto& now = fake_time[CLOCK_MONOTONIC];
    auto a = now;
    auto b = clock.next_frame_after(a);

    fake_sleep_until(b);
    fake_sleep_for(one_frame * 5 / 4);  // long render time; over a frame

    auto c = clock.next_frame_after(b);
    EXPECT_EQ(one_frame, c - b);

    fake_sleep_until(c);
    fake_sleep_for(one_frame * 7 / 6);  // long render time; over a frame

    auto d = clock.next_frame_after(c);
    EXPECT_EQ(one_frame, d - c);

    EXPECT_LT(d, now);

    fake_sleep_until(d);
    fake_sleep_for(one_frame/4);  // short render time

    // We can recover since we had a short render time...
    auto e = clock.next_frame_after(d);
    EXPECT_EQ(one_frame, e - d);
}

TEST_F(FrameClockTest, resuming_from_sleep_targets_the_future)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    auto& now = fake_time[CLOCK_MONOTONIC];
    PosixTimestamp a = now;
    auto b = clock.next_frame_after(a);
    fake_sleep_until(b);
    auto c = clock.next_frame_after(b);
    EXPECT_EQ(one_frame, c - b);
    fake_sleep_until(c);

    // Client idles for a while without producing new frames:
    fake_sleep_for(567 * one_frame);

    auto d = clock.next_frame_after(c);
    EXPECT_GT(d, now);  // Resumption must be in the future
    EXPECT_LE(d, now+one_frame);  // But not too far in the future
}

TEST_F(FrameClockTest, multiple_streams_in_sync)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    PosixTimestamp left;
    left = clock.next_frame_after(left);

    fake_sleep_for(one_frame / 9);

    PosixTimestamp right;
    right = clock.next_frame_after(right);

    ASSERT_EQ(left, right);
    fake_sleep_until(left);
    fake_sleep_until(right);

    left = clock.next_frame_after(left);
    fake_sleep_for(one_frame / 5);
    right = clock.next_frame_after(right);

    ASSERT_EQ(left, right);
    fake_sleep_until(left);
    fake_sleep_until(right);

    left = clock.next_frame_after(left);
    fake_sleep_for(one_frame / 7);
    right = clock.next_frame_after(right);

    ASSERT_EQ(left, right);
}

TEST_F(FrameClockTest, moving_between_displays_adapts_to_new_rate)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    auto const one_tv_frame = std::chrono::nanoseconds(1000000000L / 25);
    ASSERT_NE(one_frame, one_tv_frame);

    PosixTimestamp a;
    auto b = clock.next_frame_after(a);
    fake_sleep_until(b);
    auto c = clock.next_frame_after(b);
    EXPECT_EQ(one_frame, c - b);

    fake_sleep_until(c);

    // Window moves to a new slower display:
    clock.set_period(one_tv_frame);
    auto d = clock.next_frame_after(c);
    // Clock keeps ticking into the future for the new display:
    EXPECT_GT(d, c);
    // But not too far in the future:
    EXPECT_LE(d, c + one_tv_frame);

    // Vsync is now at the slower rate of the TV:
    fake_sleep_until(d);
    auto e = clock.next_frame_after(d);
    EXPECT_EQ(one_tv_frame, e - d);
    fake_sleep_until(e);
    auto f = clock.next_frame_after(e);
    EXPECT_EQ(one_tv_frame, f - e);

    fake_sleep_until(f);

    // Window moves back to the faster display:
    clock.set_period(one_frame);
    auto g = clock.next_frame_after(f);
    // Clock keeps ticking into the future for the new display:
    EXPECT_GT(g, f);
    // But not too far in the future:
    EXPECT_LE(g, f + one_tv_frame);

    // Vsync is now at the faster rate again:
    fake_sleep_until(g);
    auto h = clock.next_frame_after(g);
    EXPECT_EQ(one_frame, h - g);
    fake_sleep_until(e);
    auto i = clock.next_frame_after(h);
    EXPECT_EQ(one_frame, i - h);
}

TEST_F(FrameClockTest, resuming_comes_in_phase_with_server_vsync)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    auto& now = fake_time[CLOCK_MONOTONIC];
    PosixTimestamp a = now;
    auto b = clock.next_frame_after(a);
    fake_sleep_until(b);
    auto c = clock.next_frame_after(b);
    EXPECT_EQ(one_frame, c - b);
    fake_sleep_until(c);

    // Client idles for a while without producing new frames:
    fake_sleep_for(789 * one_frame);

    auto last_server_frame = now - 556677ns;
    clock.set_resync_callback([last_server_frame](){return last_server_frame;});

    auto d = clock.next_frame_after(c);
    EXPECT_GT(d, now);  // Resumption must be in the future
    EXPECT_LE(d, now+one_frame);  // But not too far in the future

    auto server_phase = last_server_frame % one_frame;
    EXPECT_NE(server_phase, c % one_frame);  // wasn't in phase before
    EXPECT_EQ(server_phase, d % one_frame);  // but is in phase now

    // Not only did we come in phase but we're targeting the soonest frame
    EXPECT_EQ(last_server_frame+one_frame, d);
}

TEST_F(FrameClockTest, switches_to_the_server_clock_on_startup)
{
    FrameClock clock(with_fake_time);
    clock.set_period(one_frame);

    PosixTimestamp a(CLOCK_REALTIME, 0ns);

    PosixTimestamp b;
    EXPECT_NO_THROW({
        b = clock.next_frame_after(a);
    });

    // The default resync callback uses CLOCK_MONOTONIC...
    EXPECT_NE(a.clock_id, b.clock_id);
}
