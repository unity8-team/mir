/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/server/scheduler/asio_alarm_loop.h"

#include "mir/time/high_resolution_clock.h"
#include "mir_test/auto_unblock_thread.h"
#include "mir_test/wait_object.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <array>
#include <boost/throw_exception.hpp>

#include <sys/types.h>
#include <unistd.h>

namespace mt = mir::test;

namespace
{

class AdvanceableClock : public mir::time::Clock
{
public:
    mir::time::Timestamp sample() const override
    {
        std::lock_guard<std::mutex> lock(time_mutex);
        return current_time;
    }
    void advance_by(std::chrono::milliseconds const step, mir::scheduler::AsioAlarmLoop & alarm_loop)
    {
        {
            std::lock_guard<std::mutex> lock(time_mutex);
            current_time += step;
        }

        bool done = false;
        auto evaluate_clock_alarm = alarm_loop.notify_in(
            std::chrono::milliseconds{0},
            [&done, this]
            {
                std::unique_lock<std::mutex> lock(update_time_checkpoint_mutex);
                done = true;
                update_time_checkpoint.notify_one();
            });

        std::unique_lock<std::mutex> lock(update_time_checkpoint_mutex);
        while(!done) update_time_checkpoint.wait(lock);
    }

private:
    mutable std::mutex time_mutex;

    std::mutex update_time_checkpoint_mutex;
    std::condition_variable update_time_checkpoint;


    mir::time::Timestamp current_time{
        []
        {
           mir::time::HighResolutionClock clock;
           return clock.sample();
        }()
        };
};


class AsioAlarmLoopTest : public ::testing::Test
{
public:
    std::shared_ptr<AdvanceableClock> clock = std::make_shared<AdvanceableClock>();
    mir::scheduler::AsioAlarmLoop alarm_loop{clock};
    int call_count{0};
    mt::WaitObject wait;
    std::chrono::milliseconds delay{50};

    struct UnblockAlarmLoop : mt::AutoUnblockThread
    {
        UnblockAlarmLoop(mir::scheduler::AsioAlarmLoop & alarm_loop)
            : mt::AutoUnblockThread([&alarm_loop]() {alarm_loop.stop();},
                                    [&alarm_loop]() {alarm_loop.run();})
        {}
    };
};

}

TEST_F(AsioAlarmLoopTest, runs_until_stopped)
{
    std::mutex checkpoint_mutex;
    std::condition_variable checkpoint;
    bool hit_checkpoint{false};

    auto fire_on_alarm_loop_start = alarm_loop.notify_in(
        std::chrono::milliseconds{0},
        [&checkpoint_mutex, &checkpoint, &hit_checkpoint]()
        {
            std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
            hit_checkpoint = true;
            checkpoint.notify_all();
        });

    UnblockAlarmLoop unblocker(alarm_loop);

    // TODO time dependency:
    {
        std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
        ASSERT_TRUE(checkpoint.wait_for(lock, std::chrono::milliseconds{500}, [&hit_checkpoint]() { return hit_checkpoint; }));
    }

    auto alarm = alarm_loop.notify_in(std::chrono::milliseconds{10}, [this]
    {
        wait.notify_ready();
    });

    clock->advance_by(std::chrono::milliseconds{10}, alarm_loop);
    EXPECT_NO_THROW(wait.wait_until_ready(std::chrono::milliseconds{500}));

    alarm_loop.stop();
    // Timer Service should be stopped now

    hit_checkpoint = false;
    auto should_not_fire =  alarm_loop.notify_in(std::chrono::milliseconds{0},
                                         [&checkpoint_mutex, &checkpoint, &hit_checkpoint]()
    {
        std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
        hit_checkpoint = true;
        checkpoint.notify_all();
    });

    std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
    EXPECT_FALSE(checkpoint.wait_for(lock, std::chrono::milliseconds{50}, [&hit_checkpoint]() { return hit_checkpoint; }));
}

TEST_F(AsioAlarmLoopTest, alarm_starts_in_pending_state)
{
    auto alarm = alarm_loop.notify_in(delay, [this]() {});

    UnblockAlarmLoop unblocker(alarm_loop);

    EXPECT_EQ(mir::scheduler::Alarm::pending, alarm->state());
}

TEST_F(AsioAlarmLoopTest, alarm_fires_with_correct_delay)
{
    auto alarm = alarm_loop.notify_in(delay, [](){});

    UnblockAlarmLoop unblocker(alarm_loop);

    clock->advance_by(delay - std::chrono::milliseconds{1}, alarm_loop);
    EXPECT_EQ(mir::scheduler::Alarm::pending, alarm->state());

    clock->advance_by(delay, alarm_loop);
    EXPECT_EQ(mir::scheduler::Alarm::triggered, alarm->state());
}

TEST_F(AsioAlarmLoopTest, multiple_alarms_fire)
{
    int const alarm_count{10};
    std::atomic<int> call_count{0};
    std::array<std::unique_ptr<mir::scheduler::Alarm>, alarm_count> alarms;

    for (auto& alarm : alarms)
        alarm = alarm_loop.notify_in(delay, [&call_count](){++call_count;});

    UnblockAlarmLoop unblocker(alarm_loop);
    clock->advance_by(delay, alarm_loop);

    for (auto const& alarm : alarms)
        EXPECT_EQ(mir::scheduler::Alarm::triggered, alarm->state());
}

TEST_F(AsioAlarmLoopTest, cancelled_alarm_doesnt_fire)
{
    UnblockAlarmLoop unblocker(alarm_loop);
    auto alarm = alarm_loop.notify_in(std::chrono::milliseconds{100},
                                      [](){ FAIL() << "Alarm handler of canceld alarm called";});

    EXPECT_TRUE(alarm->cancel());

    EXPECT_EQ(mir::scheduler::Alarm::cancelled, alarm->state());

    clock->advance_by(std::chrono::milliseconds{100}, alarm_loop);

    EXPECT_EQ(mir::scheduler::Alarm::cancelled, alarm->state());
}

TEST_F(AsioAlarmLoopTest, destroyed_alarm_doesnt_fire)
{
    auto alarm = alarm_loop.notify_in(std::chrono::milliseconds{200},
                                      [](){ FAIL() << "Alarm handler of destroyed alarm called"; });

    UnblockAlarmLoop unblocker(alarm_loop);

    alarm.reset(nullptr);
    clock->advance_by(std::chrono::milliseconds{200}, alarm_loop);
}

TEST_F(AsioAlarmLoopTest, rescheduled_alarm_fires_again)
{
    std::atomic<int> call_count{0};

    auto alarm = alarm_loop.notify_in(std::chrono::milliseconds{0}, [&call_count]()
    {
        if (call_count++ > 1)
            FAIL() << "Alarm called too many times";
    });

    UnblockAlarmLoop unblocker(alarm_loop);

    clock->advance_by(std::chrono::milliseconds{0}, alarm_loop);
    ASSERT_EQ(mir::scheduler::Alarm::triggered, alarm->state());

    alarm->reschedule_in(std::chrono::milliseconds{100});
    EXPECT_EQ(mir::scheduler::Alarm::pending, alarm->state());

    clock->advance_by(std::chrono::milliseconds{100}, alarm_loop);
    EXPECT_EQ(mir::scheduler::Alarm::triggered, alarm->state());
}

TEST_F(AsioAlarmLoopTest, rescheduled_alarm_cancels_previous_scheduling)
{
    std::atomic<int> call_count{0};

    auto alarm = alarm_loop.notify_in(std::chrono::milliseconds{100}, [&call_count]()
    {
        call_count++;
    });

    UnblockAlarmLoop unblocker(alarm_loop);
    clock->advance_by(std::chrono::milliseconds{90}, alarm_loop);

    EXPECT_EQ(mir::scheduler::Alarm::pending, alarm->state());
    EXPECT_EQ(0, call_count);
    EXPECT_TRUE(alarm->reschedule_in(std::chrono::milliseconds{100}));
    EXPECT_EQ(mir::scheduler::Alarm::pending, alarm->state());

    clock->advance_by(std::chrono::milliseconds{110}, alarm_loop);

    EXPECT_EQ(mir::scheduler::Alarm::triggered, alarm->state());
    EXPECT_EQ(1, call_count);
}

TEST_F(AsioAlarmLoopTest, alarm_fires_at_correct_time_point)
{
    mir::time::Timestamp real_soon = clock->sample() + std::chrono::milliseconds{120};

    auto alarm = alarm_loop.notify_at(real_soon, []{});

    UnblockAlarmLoop unblocker(alarm_loop);

    clock->advance_by(std::chrono::milliseconds{119}, alarm_loop);
    EXPECT_EQ(mir::scheduler::Alarm::pending, alarm->state());

    clock->advance_by(std::chrono::milliseconds{1}, alarm_loop);
    EXPECT_EQ(mir::scheduler::Alarm::triggered, alarm->state());
}
