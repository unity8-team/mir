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

#include "asio_server_action_queue.h"

mir::scheduler::AsioServerActionQueue::AsioServerActionQueue(boost::asio::io_service & service)
    : io(service)
{
}

mir::scheduler::AsioServerActionQueue::~AsioServerActionQueue() noexcept(true)
{
}

void mir::scheduler::AsioServerActionQueue::enqueue(void const* owner, ServerAction const& action)
{
    {
        std::lock_guard<std::mutex> lock{server_actions_mutex};
        server_actions.push_back({owner, action});
    }

    io.post([this] { process_server_actions(); });
}

void mir::scheduler::AsioServerActionQueue::pause_processing_for(void const* owner)
{
    std::lock_guard<std::mutex> lock{server_actions_mutex};
    do_not_process.insert(owner);
}

void mir::scheduler::AsioServerActionQueue::resume_processing_for(void const* owner)
{
    {
        std::lock_guard<std::mutex> lock{server_actions_mutex};
        do_not_process.erase(owner);
    }

    io.post([this] { process_server_actions(); });
}

void mir::scheduler::AsioServerActionQueue::process_server_actions()
{
    std::unique_lock<std::mutex> lock{server_actions_mutex};

    size_t i = 0;

    while (i < server_actions.size())
    {
        /* 
         * It's safe to use references to elements, since std::deque<>
         * guarantees that references remain valid after appends, which is
         * the only operation that can be performed on server_actions outside
         * this function (in AsioServerActionQueue::post()).
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
