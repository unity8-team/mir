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

#include "asio_timer_service.h"
#include "sentinel_action.h"

#include "boost/date_time/posix_time/conversion.hpp"
#include <mutex>

namespace
{
struct MirClockTimerTraits
{
    // TODO the clock used by the main loop is a global setting, this is a restriction
    // of boost::asio only allowing static methods inside the taits type.
    struct TimerServiceClockStorage
    {
    public:
        void set_clock(std::shared_ptr<mir::time::Clock const> const& clock)
        {
            std::lock_guard<std::mutex> lock(timer_service_mutex);
            auto stored_clock = timer_service_clock.lock();
            if (stored_clock && stored_clock != clock)
                BOOST_THROW_EXCEPTION(std::logic_error("A clock is already in use as time source for mir::AsioTimerService"));
            timer_service_clock = clock;
        }
        mir::time::Timestamp now()
        {
            std::lock_guard<std::mutex> lock(timer_service_mutex);
            auto clock = timer_service_clock.lock();
            if (!clock)
                BOOST_THROW_EXCEPTION(std::logic_error("No clock available to create time stamp"));
            return clock->sample();
        }
    private:
        std::mutex timer_service_mutex;
        std::weak_ptr<mir::time::Clock const> timer_service_clock;
    };

    static TimerServiceClockStorage clock_storage;

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

MirClockTimerTraits::TimerServiceClockStorage MirClockTimerTraits::clock_storage;

typedef boost::asio::basic_deadline_timer<mir::time::Timestamp, MirClockTimerTraits> deadline_timer;
}

class mir::AsioTimerService::AlarmImpl : public mir::time::Alarm
{
public:
    AlarmImpl(boost::asio::io_service& io,
              mir::AsioTimerService & service,
              std::chrono::milliseconds delay,
              std::function<void(void)> callback);

    AlarmImpl(boost::asio::io_service& io,
              mir::AsioTimerService & service,
              mir::time::Timestamp time_point,
              std::function<void(void)> callback);

    ~AlarmImpl() noexcept override;

    // mir::time::Alarm:
    bool cancel() override;
    State state() const override;

    bool reschedule_in(std::chrono::milliseconds delay) override;
    bool reschedule_for(mir::time::Timestamp time_point) override;


    // called through SentinelActions:
    void update_timer(ServerActionQueue & queue);
    bool cancel_timer();

private:
    struct InternalState
    {
        InternalState(std::function<void(void)> callback)
            : callback{callback}, state{pending}
        {
        }

        mutable std::mutex m;
        std::function<void(void)> callback;
        State state;
    };

    ::deadline_timer timer;
    std::shared_ptr<InternalState> data;
    mir::AsioTimerService & service;
};

mir::AsioTimerService::AlarmImpl::AlarmImpl(boost::asio::io_service& io,
                     mir::AsioTimerService & service,
                     std::chrono::milliseconds delay,
                     std::function<void ()> callback) :
    timer{io},
    data{std::make_shared<InternalState>(callback)},
    service(service)
{
    reschedule_in(delay);
}

mir::AsioTimerService::AlarmImpl::AlarmImpl(boost::asio::io_service& io,
                     mir::AsioTimerService & service,
                     mir::time::Timestamp time_point,
                     std::function<void ()> callback) :
    timer{io},
    data{std::make_shared<InternalState>(callback)},
    service(service)
{
    reschedule_for(time_point);
}

mir::AsioTimerService::AlarmImpl::~AlarmImpl() noexcept
{
    AlarmImpl::cancel();
}

bool mir::AsioTimerService::AlarmImpl::cancel()
{
    return service.cancel_alarm(*this);
}

bool mir::AsioTimerService::AlarmImpl::cancel_timer()
{
    std::lock_guard<decltype(data->m)> lock(data->m);
    if (data->state == triggered)
        return false;

    data->state = cancelled;
    timer.cancel();
    return true;
}

mir::time::Alarm::State mir::AsioTimerService::AlarmImpl::state() const
{
    std::lock_guard<decltype(data->m)> lock(data->m);

    return data->state;
}

bool mir::AsioTimerService::AlarmImpl::reschedule_in(std::chrono::milliseconds delay)
{
    bool cancelling = timer.expires_from_now(delay);
    service.reschedule_alarm(*this);
    return cancelling;
}

bool mir::AsioTimerService::AlarmImpl::reschedule_for(mir::time::Timestamp time_point)
{
    bool cancelling = timer.expires_at(time_point);
    service.reschedule_alarm(*this);
    return cancelling;
}

void mir::AsioTimerService::AlarmImpl::update_timer(ServerActionQueue & queue)
{
    std::lock_guard<decltype(data->m)> lock(data->m);

    // Awkwardly, we can't stop the async_wait handler from being called
    // on a destroyed AlarmImpl. This means we need to wedge a shared_ptr
    // into the async_wait callback.
    std::weak_ptr<InternalState> possible_data = data;
    timer.async_wait([possible_data,&queue](boost::system::error_code const& ec)
    {
        if (ec)
            return;

        auto data = possible_data.lock();
        if (!data)
            return;

        queue.enqueue(
            data.get(),
            [possible_data]()
            {
                auto data = possible_data.lock();
                if (!data)
                    return;

                std::unique_lock<decltype(data->m)> lock(data->m);
                if (data->state == pending)
                {
                    data->state = triggered;
                    lock.unlock();
                    data->callback();
                }
            });
        std::lock_guard<decltype(data->m)> lock(data->m);
        data->state = pending;
    });
    data->state = pending;
}

mir::AsioTimerService::AsioTimerService(std::shared_ptr<time::Clock> const& clock)
    : work{io}, clock(clock), action_queue{io}
{
    MirClockTimerTraits::set_clock(clock);
}

mir::AsioTimerService::~AsioTimerService() noexcept(true)
{
}

void mir::AsioTimerService::run()
{
    {
        std::lock_guard<std::mutex> lock(thread_id_mutex);
        timer_thread = std::this_thread::get_id();
    }
    io.run();
    std::lock_guard<std::mutex> lock(thread_id_mutex);
    timer_thread.reset();
}

void mir::AsioTimerService::stop()
{
    std::lock_guard<std::mutex> lock(thread_id_mutex);
    mir::SentinelAction(
        action_queue,
        timer_thread,
        [this]
        {
            timer_thread.reset();
            io.stop();
        });
}


std::unique_ptr<mir::time::Alarm> mir::AsioTimerService::notify_in(std::chrono::milliseconds delay,
                                                                 std::function<void()> callback)
{
    return std::unique_ptr<mir::time::Alarm>{new AlarmImpl{io, *this, delay, callback}};
}

std::unique_ptr<mir::time::Alarm> mir::AsioTimerService::notify_at(mir::time::Timestamp time_point,
                                                                 std::function<void()> callback)
{
    return std::unique_ptr<mir::time::Alarm>{new AlarmImpl{io, *this, time_point, callback}};

}

void mir::AsioTimerService::reschedule_alarm(mir::AsioTimerService::AlarmImpl& alarm)
{
    std::lock_guard<std::mutex> lock(thread_id_mutex);
    mir::SentinelAction(
        action_queue,
        timer_thread,
        [this,&alarm]
        {
            alarm.update_timer(action_queue);
        });
}

bool mir::AsioTimerService::cancel_alarm(mir::AsioTimerService::AlarmImpl& alarm)
{
    bool result;
    std::lock_guard<std::mutex> lock(thread_id_mutex);
    mir::SentinelAction(
        action_queue,
        timer_thread,
        [this, &result, &alarm]
        {
            result = alarm.cancel_timer();
        });

    return result;
}

