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

#include "src/server/asio_timer_service.h"

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
    void advance_by(std::chrono::milliseconds const step, mir::AsioTimerService & timer_service)
    {
        bool done = false;
        std::mutex checkpoint_mutex;
        std::condition_variable checkpoint;

        {
            std::lock_guard<std::mutex> lock(time_mutex);
            current_time += step;
        }
        auto evaluate_clock_alarm = timer_service.notify_in(
            std::chrono::milliseconds{0},
            [&done, &checkpoint_mutex, &checkpoint]
            {
                std::unique_lock<std::mutex> lock(checkpoint_mutex);
                done = true;
                checkpoint.notify_one();
            });

        std::unique_lock<std::mutex> lock(checkpoint_mutex);
        while(!done) checkpoint.wait(lock);
    }

private:
    mutable std::mutex time_mutex;
    mir::time::Timestamp current_time{
        []
        {
           mir::time::HighResolutionClock clock;
           return clock.sample();
        }()
        };
};


class AsioTimerServiceTest : public ::testing::Test
{
public:
    std::shared_ptr<AdvanceableClock> clock = std::make_shared<AdvanceableClock>();
    mir::AsioTimerService timer_service{clock};
    int call_count{0};
    mt::WaitObject wait;
    std::chrono::milliseconds delay{50};

    struct UnblockTimerService : mt::AutoUnblockThread
    {
        UnblockTimerService(mir::AsioTimerService & timer_service)
            : mt::AutoUnblockThread([&timer_service]() {timer_service.stop();},
                                    [&timer_service]() {timer_service.run();})
        {}
    };
};

}

TEST_F(AsioTimerServiceTest, runs_until_stop_called)
{
    std::mutex checkpoint_mutex;
    std::condition_variable checkpoint;
    bool hit_checkpoint{false};

    auto fire_on_timer_service_start = timer_service.notify_in(
        std::chrono::milliseconds{0},
        [&checkpoint_mutex, &checkpoint, &hit_checkpoint]()
        {
            std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
            hit_checkpoint = true;
            checkpoint.notify_all();
        });

    UnblockTimerService unblocker(timer_service);

    // TODO time dependency:
    {
        std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
        ASSERT_TRUE(checkpoint.wait_for(lock, std::chrono::milliseconds{500}, [&hit_checkpoint]() { return hit_checkpoint; }));
    }

    auto alarm = timer_service.notify_in(std::chrono::milliseconds{10}, [this]
    {
        wait.notify_ready();
    });

    clock->advance_by(std::chrono::milliseconds{10}, timer_service);
    EXPECT_NO_THROW(wait.wait_until_ready(std::chrono::milliseconds{500}));

    timer_service.stop();
    // Timer Service should be stopped now

    hit_checkpoint = false;
    auto should_not_fire =  timer_service.notify_in(std::chrono::milliseconds{0},
                                         [&checkpoint_mutex, &checkpoint, &hit_checkpoint]()
    {
        std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
        hit_checkpoint = true;
        checkpoint.notify_all();
    });

    std::unique_lock<decltype(checkpoint_mutex)> lock(checkpoint_mutex);
    EXPECT_FALSE(checkpoint.wait_for(lock, std::chrono::milliseconds{50}, [&hit_checkpoint]() { return hit_checkpoint; }));
}

TEST_F(AsioTimerServiceTest, alarm_starts_in_pending_state)
{
    auto alarm = timer_service.notify_in(delay, [this]() {});

    UnblockTimerService unblocker(timer_service);

    EXPECT_EQ(mir::time::Alarm::pending, alarm->state());
}

TEST_F(AsioTimerServiceTest, alarm_fires_with_correct_delay)
{
    UnblockTimerService unblocker(timer_service);

    auto alarm = timer_service.notify_in(delay, [](){});

    clock->advance_by(delay - std::chrono::milliseconds{1}, timer_service);
    EXPECT_EQ(mir::time::Alarm::pending, alarm->state());

    clock->advance_by(delay, timer_service);
    EXPECT_EQ(mir::time::Alarm::triggered, alarm->state());
}

TEST_F(AsioTimerServiceTest, multiple_alarms_fire)
{
    int const alarm_count{10};
    std::atomic<int> call_count{0};
    std::array<std::unique_ptr<mir::time::Alarm>, alarm_count> alarms;

    for (auto& alarm : alarms)
        alarm = timer_service.notify_in(delay, [&call_count](){++call_count;});

    UnblockTimerService unblocker(timer_service);
    clock->advance_by(delay, timer_service);

    for (auto const& alarm : alarms)
        EXPECT_EQ(mir::time::Alarm::triggered, alarm->state());
}

TEST_F(AsioTimerServiceTest, cancelled_alarm_doesnt_fire)
{
    UnblockTimerService unblocker(timer_service);
    auto alarm = timer_service.notify_in(std::chrono::milliseconds{100},
                              [](){ FAIL() << "Alarm handler of canceld alarm called";});

    EXPECT_TRUE(alarm->cancel());

    EXPECT_EQ(mir::time::Alarm::cancelled, alarm->state());

    clock->advance_by(std::chrono::milliseconds{100}, timer_service);

    EXPECT_EQ(mir::time::Alarm::cancelled, alarm->state());
}

TEST_F(AsioTimerServiceTest, destroyed_alarm_doesnt_fire)
{
    auto alarm = timer_service.notify_in(std::chrono::milliseconds{200},
                              [](){ FAIL() << "Alarm handler of destroyed alarm called"; });

    UnblockTimerService unblocker(timer_service);

    alarm.reset(nullptr);
    clock->advance_by(std::chrono::milliseconds{200}, timer_service);
}

TEST_F(AsioTimerServiceTest, rescheduled_alarm_fires_again)
{
    std::atomic<int> call_count{0};

    auto alarm = timer_service.notify_in(std::chrono::milliseconds{0}, [&call_count]()
    {
        if (call_count++ > 1)
            FAIL() << "Alarm called too many times";
    });

    UnblockTimerService unblocker(timer_service);

    clock->advance_by(std::chrono::milliseconds{0}, timer_service);
    ASSERT_EQ(mir::time::Alarm::triggered, alarm->state());

    alarm->reschedule_in(std::chrono::milliseconds{100});
    EXPECT_EQ(mir::time::Alarm::pending, alarm->state());

    clock->advance_by(std::chrono::milliseconds{100}, timer_service);
    EXPECT_EQ(mir::time::Alarm::triggered, alarm->state());
}

TEST_F(AsioTimerServiceTest, rescheduled_alarm_cancels_previous_scheduling)
{
    std::atomic<int> call_count{0};

    auto alarm = timer_service.notify_in(std::chrono::milliseconds{100}, [&call_count]()
    {
        call_count++;
    });

    UnblockTimerService unblocker(timer_service);
    clock->advance_by(std::chrono::milliseconds{90}, timer_service);

    EXPECT_EQ(mir::time::Alarm::pending, alarm->state());
    EXPECT_EQ(0, call_count);
    EXPECT_TRUE(alarm->reschedule_in(std::chrono::milliseconds{100}));
    EXPECT_EQ(mir::time::Alarm::pending, alarm->state());

    clock->advance_by(std::chrono::milliseconds{110}, timer_service);

    EXPECT_EQ(mir::time::Alarm::triggered, alarm->state());
    EXPECT_EQ(1, call_count);
}

TEST_F(AsioTimerServiceTest, alarm_fires_at_correct_time_point)
{
    mir::time::Timestamp real_soon = clock->sample() + std::chrono::milliseconds{120};

    auto alarm = timer_service.notify_at(real_soon, []{});

    UnblockTimerService unblocker(timer_service);

    clock->advance_by(std::chrono::milliseconds{119}, timer_service);
    EXPECT_EQ(mir::time::Alarm::pending, alarm->state());

    clock->advance_by(std::chrono::milliseconds{1}, timer_service);
    EXPECT_EQ(mir::time::Alarm::triggered, alarm->state());
}
