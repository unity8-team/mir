/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_DISPATCH_DISPATCHABLE_H_
#define MIR_DISPATCH_DISPATCHABLE_H_

#include "mir/fd.h"

namespace mir
{
namespace dispatch
{

enum fd_event : uint32_t {
    readable =      1<<0,
    writable =      1<<1,
    remote_closed = 1<<2,
    error =         1<<3
};

using fd_events = uint32_t;

class Dispatchable
{
public:
    Dispatchable() = default;
    virtual ~Dispatchable() = default;

    Dispatchable& operator=(Dispatchable const&) = delete;
    Dispatchable(Dispatchable const&) = delete;

    /**
     * \brief Get a poll()able file descriptor
     * \return A file descriptor usable with poll() or equivalent function calls that
     *         becomes readable when there are dispatchable events
     */
    virtual Fd watch_fd() const = 0;

    /**
     * \brief Dispatch one pending event
     * \param [in] event    The set of events current on the file-descriptor
     * \returns False iff no more events will be produced by this Dispatchable
     *          Dispatch should no longer be called.
     * \note This will dispatch at most one event. If there are multiple events
     *       specify in \ref event (eg: readable | remote_closed) then dispatch
     *       will process only one.
     * \note It is harmless to call dispatch() with an event that does not contain
     *       any of the events from relevant_events(). The function will do
     *       nothing in such a case.
     */
    virtual bool dispatch(fd_events events) = 0;

    /**
     * \brief The set of file-descriptor events this Dispatchable handles
     */
    virtual fd_events relevant_events() const = 0;
};
}
}

#endif // MIR_DISPATCH_DISPATCHABLE_H_