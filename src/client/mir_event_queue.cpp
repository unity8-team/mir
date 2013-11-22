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
    : running(true), interval_ms(-1)
{
}

void MirEventQueue::animate(int milliseconds)
{
    Lock lock(guard);
    interval_ms = milliseconds;
    deadline = std::chrono::system_clock::now() +
               std::chrono::milliseconds(interval_ms);
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

bool MirEventQueue::wait(MirEvent* e)
{
    Lock lock(guard);

    while (running && queue.empty() && interval_ms)
    {
        if (interval_ms < 0)
            cond.wait(lock);
        else if (cond.wait_until(lock, deadline) == std::cv_status::timeout)
            break;
    }

    bool pending = !queue.empty();
    if (pending)
    {
        *e = queue.front();
        queue.pop_front();
    }
    else
    {
        e->type = mir_event_type_null;
        if (interval_ms > 0)
            deadline += std::chrono::milliseconds(interval_ms);
    }

    // Only break the client's event loop after it has chosen to quit() and
    // there are no more events to process.
    return running || pending;
}

