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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 *              Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_ASIO_SERVER_ACTION_QUEUE
#define MIR_ASIO_SERVER_ACTION_QUEUE

#include "mir/server_action_queue.h"

#include <boost/asio.hpp>

#include <mutex>
#include <condition_variable>
#include <deque>
#include <set>

namespace mir
{

class AsioServerActionQueue : public ServerActionQueue
{
public:
    explicit AsioServerActionQueue(boost::asio::io_service & service);
    ~AsioServerActionQueue() noexcept(true);

    void enqueue(void const* owner, ServerAction const& action) override;
    void pause_processing_for(void const* owner) override;
    void resume_processing_for(void const* owner) override;

private:

    bool should_process(void const*);
    void process_server_actions();

    boost::asio::io_service & io;
    std::mutex server_actions_mutex;
    std::deque<std::pair<void const*,ServerAction>> server_actions;
    std::set<void const*> do_not_process;
};

}

#endif
