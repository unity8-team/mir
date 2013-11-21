/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir_event_queue.h"

namespace
{
typedef std::unique_lock<std::mutex> Lock;
}

MirEventQueue::MirEventQueue()
    : running(true), handled(false)
{
}

void MirEventQueue::push(Event const& e)
{
    Lock lock(guard);
    if (running)
    {
        queue.push_back(e);
        cond.notify_one();
    } // else events after quit() are ignored.
}

void MirEventQueue::quit()
{
    Lock lock(guard);
    running = false;
    cond.notify_all();
}

bool MirEventQueue::wait(std::chrono::milliseconds timeout, Event const** e)
{
    Lock lock(guard);

    if (handled)
        queue.pop_front();  // Remove the previous one handled

    handled = false;

    auto now = std::chrono::system_clock::now();
    auto deadline = now + timeout;

    while (running &&
           cond.wait_until(lock, deadline) == std::cv_status::no_timeout &&
           queue.empty())
    {
    }

    bool pending = !queue.empty();
    if (pending && e)
    {
        *e = &queue.front();
        handled = true;
    }

    return pending;
}

