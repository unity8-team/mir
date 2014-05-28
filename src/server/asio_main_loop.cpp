/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/asio_main_loop.h"

#include "boost/date_time/posix_time/conversion.hpp"

#include <cassert>
#include <mutex>
#include <condition_variable>

namespace bap = boost::asio::posix;

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
                BOOST_THROW_EXCEPTION(std::logic_error("A clock is already in use as time source for mir::AsioMainLoop"));
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

class mir::AsioMainLoop::SignalHandler
{
public:
    SignalHandler(boost::asio::io_service& io,
                  std::initializer_list<int> signals,
                  std::function<void(int)> const& handler)
        : signal_set{io},
          handler{handler}
    {
        for (auto sig : signals)
            signal_set.add(sig);
    }

    void async_wait()
    {
        signal_set.async_wait(
            std::bind(&SignalHandler::handle, this,
                      std::placeholders::_1, std::placeholders::_2));
    }

private:
    void handle(boost::system::error_code err, int sig)
    {
        if (!err)
        {
            handler(sig);
            signal_set.async_wait(
                std::bind(&SignalHandler::handle, this,
                          std::placeholders::_1, std::placeholders::_2));
        }
    }

    boost::asio::signal_set signal_set;
    std::function<void(int)> handler;
};

class mir::AsioMainLoop::FDHandler
{
public:
    typedef std::unique_ptr<bap::stream_descriptor> stream_descriptor_ptr;

    FDHandler(boost::asio::io_service& io,
              std::initializer_list<int> fds,
              void const* owner,
              std::function<void(int)> const& handler)
        : data{std::make_shared<InternalState>(io, fds, handler)}, owner{owner}
    {
    }

    bool is_owned_by(void const* possible_owner) const
    {
        return owner == possible_owner;
    }

    bool empty() const
    {
        return data->stream_descriptors.empty();
    }

    void reduce_set(std::initializer_list<int> fds)
    {
        std::lock_guard<std::mutex> descriptors_guard(data->stream_descriptors_mutex);
        auto end_of_valid = remove_if(begin(data->stream_descriptors),
              end(data->stream_descriptors),
              [&fds,this](stream_descriptor_ptr const& item)
              {
                  return cancel_if_in_list(fds, *item);
              });
        data->stream_descriptors.erase(end_of_valid, end(data->stream_descriptors));
    }

    void async_wait()
    {
        std::lock_guard<std::mutex> descriptors_guard(data->stream_descriptors_mutex);
        for (auto const& s : data->stream_descriptors)
            read_some(s.get(), data);
    }

private:
    bool cancel_if_in_list(std::initializer_list<int> fds, bap::stream_descriptor & item)
    {
        auto native = item.native();
        for (int fd : fds)
            if (fd == native)
            {
                cancel_pending_waits(item);
                // MainLoop does not own the fds
                item.release();
                return true;
            }
        return false;
    }

    void cancel_pending_waits(bap::stream_descriptor & s)
    {
        try
        {
            s.cancel();
        }
        catch(boost::system::system_error const& error)
        {
            // TODO log error
        }
    }

    struct InternalState
    {
        std::mutex stream_descriptors_mutex;
        std::vector<stream_descriptor_ptr> stream_descriptors;
        std::function<void(int)> const handler;

        InternalState(boost::asio::io_service & io, std::initializer_list<int> fds,
                      std::function<void(int)> const& handler)
            : handler{handler}
        {
            for (auto fd : fds)
                stream_descriptors.emplace_back(new bap::stream_descriptor{io, fd});
        }

        bool still_registered(bap::stream_descriptor* s)
        {
            return end(stream_descriptors) != find_if(
                begin(stream_descriptors),
                end(stream_descriptors),
                [s](stream_descriptor_ptr const& item)
                {
                    return item.get() == s;
                });
        }
    };

    static void read_some(bap::stream_descriptor* s, std::weak_ptr<InternalState> const& data)
    {
        s->async_read_some(
            boost::asio::null_buffers(),
            [data,s](boost::system::error_code err, size_t /*bytes*/)
            {
                if (err)
                    return;
                auto state = data.lock();
                std::unique_lock<std::mutex> lock(state->stream_descriptors_mutex);
                if (state->still_registered(s))
                {
                    lock.unlock();
                    state->handler(s->native_handle());

                    lock.lock();
                    if (state->still_registered(s))
                        read_some(s, data);
                }
            });
    }

    std::shared_ptr<InternalState> data;
    void const* owner;
};

/*
 * We need to define the constructor and destructor in the .cpp file,
 * so that we can use unique_ptr to hold SignalHandler. Otherwise, users
 * of AsioMainLoop end up creating constructors and destructors that
 * don't have complete type information for SignalHandler and fail
 * to compile.
 */
mir::AsioMainLoop::AsioMainLoop(std::shared_ptr<time::Clock> const& clock)
    : work{io}, clock(clock)
{
    MirClockTimerTraits::set_clock(clock);
}

mir::AsioMainLoop::~AsioMainLoop() noexcept(true)
{
}

void mir::AsioMainLoop::run()
{
    io.run();
}

void mir::AsioMainLoop::stop()
{
    io.stop();
}

void mir::AsioMainLoop::register_signal_handler(
    std::initializer_list<int> signals,
    std::function<void(int)> const& handler)
{
    assert(handler);

    auto sig_handler = std::unique_ptr<SignalHandler>{
        new SignalHandler{io, signals, handler}};

    sig_handler->async_wait();

    signal_handlers.push_back(std::move(sig_handler));
}

void mir::AsioMainLoop::register_fd_handler(
    std::initializer_list<int> fds,
    void const* owner,
    std::function<void(int)> const& handler)
{
    assert(handler);

    auto fd_handler = std::unique_ptr<FDHandler>{
        new FDHandler{io, fds, owner, handler}};

    fd_handler->async_wait();

    std::lock_guard<std::mutex> lock(fd_handlers_mutex);
    fd_handlers.push_back(std::move(fd_handler));
}

void mir::AsioMainLoop::unregister_fd_handler(
    std::initializer_list<int> fds,
    void const* owner)
{
    std::lock_guard<std::mutex> lock(fd_handlers_mutex);
    auto end_of_valid = remove_if(
        begin(fd_handlers), end(fd_handlers),
        [owner,&fds](std::unique_ptr<FDHandler> & item)
        {
            if (item->is_owned_by(owner))
            {
                item->reduce_set(fds);
            }

            return item->empty();
        });
    fd_handlers.erase(end_of_valid, end(fd_handlers));
}

namespace
{
class AlarmImpl : public mir::time::Alarm
{
public:
    AlarmImpl(boost::asio::io_service& io,
              std::chrono::milliseconds delay,
              std::function<void(void)> callback);

    AlarmImpl(boost::asio::io_service& io,
              mir::time::Timestamp time_point,
              std::function<void(void)> callback);

    ~AlarmImpl() noexcept override;

    bool cancel() override;
    State state() const override;

    bool reschedule_in(std::chrono::milliseconds delay) override;
    bool reschedule_for(mir::time::Timestamp time_point) override;
private:
    void update_timer();
    struct InternalState
    {
        explicit InternalState(std::function<void(void)> callback)
            : callback{callback}
        {
        }

        mutable std::mutex m;
        std::function<void(void)> callback;
        State state;
    };

    ::deadline_timer timer;
    std::shared_ptr<InternalState> data;
};

AlarmImpl::AlarmImpl(boost::asio::io_service& io,
                     std::chrono::milliseconds delay,
                     std::function<void ()> callback)
    : timer{io},
      data{std::make_shared<InternalState>(callback)}
{
    reschedule_in(delay);
}

AlarmImpl::AlarmImpl(boost::asio::io_service& io,
                     mir::time::Timestamp time_point,
                     std::function<void ()> callback)
    : timer{io},
      data{std::make_shared<InternalState>(callback)}
{
    reschedule_for(time_point);
}

AlarmImpl::~AlarmImpl() noexcept
{
    AlarmImpl::cancel();
}

bool AlarmImpl::cancel()
{
    std::lock_guard<decltype(data->m)> lock(data->m);
    if (data->state == triggered)
        return false;

    data->state = cancelled;
    timer.cancel();
    return true;
}

mir::time::Alarm::State AlarmImpl::state() const
{
    std::lock_guard<decltype(data->m)> lock(data->m);

    return data->state;
}

bool AlarmImpl::reschedule_in(std::chrono::milliseconds delay)
{
    bool cancelling = timer.expires_from_now(delay);
    update_timer();
    return cancelling;
}

bool AlarmImpl::reschedule_for(mir::time::Timestamp time_point)
{
    bool cancelling = timer.expires_at(time_point);
    update_timer();
    return cancelling;
}

void AlarmImpl::update_timer()
{
    std::lock_guard<decltype(data->m)> lock(data->m);
    // Awkwardly, we can't stop the async_wait handler from being called
    // on a destroyed AlarmImpl. This means we need to wedge a shared_ptr
    // into the async_wait callback.
    std::weak_ptr<InternalState> possible_data = data;
    timer.async_wait([possible_data](boost::system::error_code const& ec)
    {
        auto data = possible_data.lock();
        if (!data)
            return;

        std::unique_lock<decltype(data->m)> lock(data->m);
        if (!ec && data->state == pending)
        {
            data->state = triggered;
            lock.unlock();
            data->callback();
        }
    });
    data->state = pending;
}
}

std::unique_ptr<mir::time::Alarm> mir::AsioMainLoop::notify_in(std::chrono::milliseconds delay,
                                                               std::function<void()> callback)
{
    return std::unique_ptr<mir::time::Alarm>{new AlarmImpl{io, delay, callback}};
}

std::unique_ptr<mir::time::Alarm> mir::AsioMainLoop::notify_at(mir::time::Timestamp time_point,
                                                               std::function<void()> callback)
{
    return std::unique_ptr<mir::time::Alarm>{new AlarmImpl{io, time_point, callback}};

}
void mir::AsioMainLoop::enqueue(void const* owner, ServerAction const& action)
{
    {
        std::lock_guard<std::mutex> lock{server_actions_mutex};
        server_actions.push_back({owner, action});
    }

    io.post([this] { process_server_actions(); });
}

void mir::AsioMainLoop::pause_processing_for(void const* owner)
{
    std::lock_guard<std::mutex> lock{server_actions_mutex};
    do_not_process.insert(owner);
}

void mir::AsioMainLoop::resume_processing_for(void const* owner)
{
    {
        std::lock_guard<std::mutex> lock{server_actions_mutex};
        do_not_process.erase(owner);
    }

    io.post([this] { process_server_actions(); });
}

void mir::AsioMainLoop::process_server_actions()
{
    std::unique_lock<std::mutex> lock{server_actions_mutex};

    size_t i = 0;

    while (i < server_actions.size())
    {
        /* 
         * It's safe to use references to elements, since std::deque<>
         * guarantees that references remain valid after appends, which is
         * the only operation that can be performed on server_actions outside
         * this function (in AsioMainLoop::post()).
         */
        auto const& owner = server_actions[i].first;
        auto const& action = server_actions[i].second;

        if (do_not_process.find(owner) == do_not_process.end())
        {
            lock.unlock();
            action();
            lock.lock();
            /*
             * This erase is always ok, since outside this function
             * we only append to server_actions, i.e., our index i
             * is guaranteed to remain valid and correct.
             */
            server_actions.erase(server_actions.begin() + i);
        }
        else
        {
            ++i;
        }
    }
}
