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

#include "asio_main_loop.h"

#include "synchronous_server_action.h"

#include <cassert>
#include <mutex>
#include <condition_variable>

namespace bap = boost::asio::posix;

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
        : handler{handler}, owner{owner}
    {
        for (auto fd : fds)
            stream_descriptors.emplace_back(new bap::stream_descriptor{io, fd});
    }

    bool is_owned_by(void const* possible_owner) const
    {
        return owner == possible_owner;
    }

    static void async_wait(std::shared_ptr<FDHandler> const& fd_handler, ServerActionQueue & queue)
    {
        for (auto const& s : fd_handler->stream_descriptors)
            read_some(s.get(), fd_handler, queue);
    }

private:
    static void read_some(bap::stream_descriptor* s, std::weak_ptr<FDHandler> const& possible_fd_handler, ServerActionQueue & queue)
    {
        s->async_read_some(
            boost::asio::null_buffers(),
            [possible_fd_handler,s,&queue](boost::system::error_code err, size_t /*bytes*/)
            {
                if (err)
                    return;

                queue.enqueue(
                    s,
                    [possible_fd_handler,s,&queue]()
                    {
                        auto fd_handler = possible_fd_handler.lock();
                        if (!fd_handler)
                            return;

                        fd_handler->handler(s->native_handle());
                        fd_handler.reset();

                        if (possible_fd_handler.lock())
                            read_some(s, possible_fd_handler, queue);
                    });
            });
    }

    std::vector<stream_descriptor_ptr> stream_descriptors;
    std::function<void(int)> const handler;
    void const* owner;
};

/*
 * We need to define the constructor and destructor in the .cpp file,
 * so that we can use unique_ptr to hold SignalHandler. Otherwise, users
 * of AsioMainLoop end up creating constructors and destructors that
 * don't have complete type information for SignalHandler and fail
 * to compile.
 */
mir::AsioMainLoop::AsioMainLoop()
    : work{io}, action_queue{io}
{
}

mir::AsioMainLoop::~AsioMainLoop() noexcept(true)
{
}

void mir::AsioMainLoop::run()
{
    main_loop_thread = std::this_thread::get_id();
    io.run();
}

void mir::AsioMainLoop::stop()
{
    io.stop();
    main_loop_thread.reset();
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

    auto fd_handler = std::make_shared<FDHandler>(io, fds, owner, handler);

    FDHandler::async_wait(fd_handler, action_queue);

    std::lock_guard<std::mutex> lock(fd_handlers_mutex);
    fd_handlers.push_back(fd_handler);
}

void mir::AsioMainLoop::unregister_fd_handler(void const* owner)
{
    SynchronousServerAction unregister{
        action_queue,
        main_loop_thread,
        [this,owner]()
        {
            std::lock_guard<std::mutex> lock(fd_handlers_mutex);
            auto end_of_valid = remove_if(
                begin(fd_handlers),
                end(fd_handlers),
                [owner](std::shared_ptr<FDHandler> const& item)
                {
                    return item->is_owned_by(owner);
                });
            fd_handlers.erase(end_of_valid, end(fd_handlers));
        }};
}

void mir::AsioMainLoop::enqueue(void const* owner, ServerAction const& action)
{
    action_queue.enqueue(owner, action);
}

void mir::AsioMainLoop::pause_processing_for(void const* owner)
{
    action_queue.pause_processing_for(owner);
}

void mir::AsioMainLoop::resume_processing_for(void const* owner)
{
    action_queue.resume_processing_for(owner);
}
