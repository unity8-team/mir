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

#ifndef MIR_ASIO_MAIN_LOOP_H_
#define MIR_ASIO_MAIN_LOOP_H_

#include "mir/main_loop.h"

#include "asio_server_action_queue.h"

#include <boost/asio.hpp>
#include <boost/optional.hpp>

#include <memory>
#include <thread>
#include <vector>
#include <mutex>
#include <utility>
#include <deque>
#include <set>

namespace mir
{

class AsioMainLoop : public MainLoop
{
public:
    AsioMainLoop();
    ~AsioMainLoop() noexcept(true);

    void run();
    void stop();

    void register_signal_handler(
        std::initializer_list<int> signals,
        std::function<void(int)> const& handler);

    void register_fd_handler(
        std::initializer_list<int> fd,
        void const* owner,
        std::function<void(int)> const& handler) override;

    void unregister_fd_handler(void const* owner) override;

    void enqueue(void const* owner, ServerAction const& action) override;
    void pause_processing_for(void const* owner) override;
    void resume_processing_for(void const* owner) override;

private:
    class SignalHandler;
    class FDHandler;

    boost::asio::io_service io;
    boost::asio::io_service::work work;
    boost::optional<std::thread::id> main_loop_thread;
    std::vector<std::unique_ptr<SignalHandler>> signal_handlers;
    std::vector<std::shared_ptr<FDHandler>> fd_handlers;
    std::mutex fd_handlers_mutex;
    AsioServerActionQueue action_queue;
};

}

#endif /* MIR_ASIO_MAIN_LOOP_H */
