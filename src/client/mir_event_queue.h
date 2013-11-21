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

#ifndef MIR_EVENT_QUEUE_H_
#define MIR_EVENT_QUEUE_H_

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <list>
#include "mir_toolkit/client_types.h"
#include "mir_toolkit/event.h"

/**
 * \addtogroup mir_toolkit
 * @{
 */
struct MirEventQueue
{
public:
    struct Event
    {
        MirEvent event;
        MirSurface* surface;
    };

    MirEventQueue();
    void animate(std::chrono::milliseconds period);
    void push(Event const& e);
    void quit();
    bool wait(Event const** e);

private:
    typedef std::list<Event> Queue;  // elements never move in memory
    std::chrono::system_clock::time_point woke;
    std::mutex guard;
    std::condition_variable cond;
    bool running;
    bool handled;
    std::chrono::milliseconds interval;
    Queue queue;
};
/**@}*/

#endif /* MIR_EVENT_QUEUE_H_ */
