/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#include "asio_alarm_loop.h"

#include "mir/time/clock.h"

#include "boost/date_time/posix_time/conversion.hpp"
#include <mutex>
#include <atomic>

namespace
{
struct MirClockTimerTraits
{
    // TODO the clock used by the timer service is a global setting, this is a restriction
    // of boost::asio only allowing static methods inside the traits type.
    struct AlarmLoopClockStorage
    {
    public:
        void set_clock(std::shared_ptr<mir::time::Clock const> const& clock)
        {
            std::lock_guard<std::mutex> lock(alarm_service_mutex);
            auto stored_clock = alarm_service_clock.lock();
            if (stored_clock && stored_clock != clock)
                BOOST_THROW_EXCEPTION(std::logic_error("A clock is already in use as time source for mir::scheduler::AsioAlarmLoop"));
            alarm_service_clock = clock;
        }
        mir::time::Timestamp now()
        {
            std::lock_guard<std::mutex> lock(alarm_service_mutex);
            auto clock = alarm_service_clock.lock();
            if (!clock)
                BOOST_THROW_EXCEPTION(std::logic_error("No clock available to create time stamp"));
            return clock->sample();
        }
    private:
        std::mutex alarm_service_mutex;
        std::weak_ptr<mir::time::Clock const> alarm_service_clock;
    };

    static AlarmLoopClockStorage clock_storage;

    static void set_clock(std::shared_ptr<mir::time::Clock const> const& clock)
    {
        clock_storage.set_clock(clock);
    }

    // time_traits interface required by boost::asio::deadline_timer{_service}
    typedef mir::time::Timestamp time_type;
    typedef std::chrono::milliseconds duration_type;


    static time_type now()
    {
        return clock_storage.now();
    }

    static time_type add(const time_type& t, const duration_type& d)
    {
        return t + d;
    }

    static duration_type subtract(const time_type& t1, const time_type& t2)
    {
        return std::chrono::duration_cast<duration_type>(t1 - t2);
    }

    static bool less_than(const time_type& t1, const time_type& t2)
    {
        return t1 < t2;
    }

    static boost::posix_time::time_duration to_posix_duration(
        const duration_type& d)
    {
        return boost::posix_time::millisec(d.count());
    }
};

MirClockTimerTraits::AlarmLoopClockStorage MirClockTimerTraits::clock_storage;

typedef boost::asio::basic_deadline_timer<mir::time::Timestamp, MirClockTimerTraits> deadline_timer;
}

class mir::scheduler::AsioAlarmLoop::AlarmImpl : public mir::scheduler::Alarm
{
public:
    AlarmImpl(boost::asio::io_service& io,
              std::chrono::milliseconds delay,
              std::function<void(void)> callback);

    AlarmImpl(boost::asio::io_service& io,
              mir::time::Timestamp time_point,
              std::function<void(void)> callback);

    AlarmImpl(boost::asio::io_service& io,
              std::function<void(void)> callback);

    ~AlarmImpl() noexcept override;

    // mir::scheduler::Alarm:
    bool cancel() override;
    State state() const override;

    bool reschedule_in(std::chrono::milliseconds delay) override;
    bool reschedule_for(mir::time::Timestamp time_point) override;

private:
    void update_timer();

    struct InternalState
    {
        InternalState(std::function<void(void)> callback)
            : callback{callback}, state{pending}
        {
        }

        std::function<void(void)> callback;
        std::atomic<State> state;
    };

    ::deadline_timer timer;
    std::shared_ptr<InternalState> data;
};

mir::scheduler::AsioAlarmLoop::AlarmImpl::AlarmImpl(boost::asio::io_service& io,
                                                    std::chrono::milliseconds delay,
                                                    std::function<void()> callback)
    : AlarmImpl(io, callback)
{
    reschedule_in(delay);
}

mir::scheduler::AsioAlarmLoop::AlarmImpl::AlarmImpl(boost::asio::io_service& io,
                                                    mir::time::Timestamp time_point,
                                                    std::function<void()> callback)
    : AlarmImpl(io, callback)
{
    reschedule_for(time_point);
}

mir::scheduler::AsioAlarmLoop::AlarmImpl::AlarmImpl(boost::asio::io_service& io, std::function<void(void)> callback)
    : timer{io}, data{std::make_shared<InternalState>(callback)}
{
    data->state = triggered;
}

mir::scheduler::AsioAlarmLoop::AlarmImpl::~AlarmImpl() noexcept
{
    AlarmImpl::cancel();
}

bool mir::scheduler::AsioAlarmLoop::AlarmImpl::cancel()
{
    State expected_state = pending;
    if (data->state.compare_exchange_strong(expected_state, cancelled))
    {
        timer.cancel();
        return true;
    }
    return false;
}

mir::scheduler::Alarm::State mir::scheduler::AsioAlarmLoop::AlarmImpl::state() const
{
    return data->state;
}

bool mir::scheduler::AsioAlarmLoop::AlarmImpl::reschedule_in(std::chrono::milliseconds delay)
{
    bool cancelling = timer.expires_from_now(delay);
    update_timer();
    return cancelling;
}

bool mir::scheduler::AsioAlarmLoop::AlarmImpl::reschedule_for(mir::time::Timestamp time_point)
{
    bool cancelling = timer.expires_at(time_point);
    update_timer();
    return cancelling;
}

void mir::scheduler::AsioAlarmLoop::AlarmImpl::update_timer()
{
    auto new_internal_state = std::make_shared<InternalState>(data->callback);

    // Awkwardly, we can't stop the async_wait handler from being called
    // on a destroyed AlarmImpl. This means we need to wedge a shared_ptr
    // into the async_wait callback.
    std::weak_ptr<InternalState> possible_data = new_internal_state;
    timer.async_wait([possible_data](boost::system::error_code const& ec)
                     {
                         if (ec)
                             return;

                         auto data = possible_data.lock();
                         if (!data)
                             return;

                         State expected_state = pending;
                         if (data->state.compare_exchange_strong(expected_state, triggered))
                             data->callback();
                     });

    data = new_internal_state;
}

mir::scheduler::AsioAlarmLoop::AsioAlarmLoop(std::shared_ptr<time::Clock> const& clock) : work{io}, clock(clock)
{
    MirClockTimerTraits::set_clock(clock);
}

mir::scheduler::AsioAlarmLoop::~AsioAlarmLoop() noexcept(true)
{
}

void mir::scheduler::AsioAlarmLoop::run()
{
    io.run();
}

void mir::scheduler::AsioAlarmLoop::stop()
{
    io.stop();
}

std::unique_ptr<mir::scheduler::Alarm> mir::scheduler::AsioAlarmLoop::notify_in(std::chrono::milliseconds delay,
                                                                                std::function<void()> callback)
{
    return std::unique_ptr<mir::scheduler::Alarm>{new AlarmImpl{io, delay, callback}};
}

std::unique_ptr<mir::scheduler::Alarm> mir::scheduler::AsioAlarmLoop::notify_at(mir::time::Timestamp time_point,
                                                                                std::function<void()> callback)
{
    return std::unique_ptr<mir::scheduler::Alarm>{new AlarmImpl{io, time_point, callback}};
}

std::unique_ptr<mir::scheduler::Alarm> mir::scheduler::AsioAlarmLoop::create_alarm(std::function<void()> callback)
{
    return std::unique_ptr<mir::scheduler::Alarm>{new AlarmImpl{io, callback}};
}
