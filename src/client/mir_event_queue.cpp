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
    : running(true), handled(false), interval(0)
{
}

void MirEventQueue::animate(std::chrono::milliseconds period)
{
    Lock lock(guard);
    interval = period;
    woke = std::chrono::system_clock::now() - interval;
    cond.notify_all();
}

void MirEventQueue::push(MirEvent const* e)
{
    Lock lock(guard);
    if (running)
    {
        queue.push_back(*e);
        cond.notify_one();
    } // else events after quit() are ignored.
}

void MirEventQueue::quit()
{
    Lock lock(guard);
    running = false;
    cond.notify_all();
}

bool MirEventQueue::wait(MirEvent const** e)
{
    Lock lock(guard);

    if (handled)
        queue.pop_front();  // Remove the previous one handled

    std::chrono::system_clock::time_point deadline;
    do
    {
        deadline = woke + interval;
    } while (running &&
             queue.empty() &&
             cond.wait_until(lock, deadline) == std::cv_status::no_timeout);

    handled = !queue.empty();
    if (e)
    {
        if (handled)
        {
            *e = &queue.front();
        }
        else
        {
            *e = nullptr;
            woke = std::chrono::system_clock::now();
        }
    }

    // Only break the client's event loop after it has chosen to quit() and
    // there are no more events to process.
    return running || handled;
}

