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

    void push(Event const& e);
    void quit();

    /**
     * Wait for the next event (if any).
     *   \param [in] timeout  Maximum time to wait for
     *   \param [out] e       Returns a pointer to the next event in the
     *                        queue (if any). This pointer remains valid until
     *                        the next call to wait().
     *   \return              True until quit() has been called and there are
     *                        no more events pending.
     */
    bool wait(std::chrono::milliseconds timeout, Event const** e);

private:
    typedef std::list<Event> Queue;  // elements never move in memory

    std::mutex guard;
    std::condition_variable cond;
    bool running;
    bool handled;

    Queue queue;
};
/**@}*/

#endif /* MIR_EVENT_QUEUE_H_ */
