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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_INPUT_SEND_OBSERVER_H
#define MIR_INPUT_INPUT_SEND_OBSERVER_H

#include <memory>

namespace mir
{
namespace input
{
class InputSendEntry;

class InputSendObserver
{
public:
    virtual ~InputSendObserver() = default;

    /*!
     * \brief An attempt to send an input event to a destination failed
     *
     * TODO failure reason
     */
    virtual void send_failed(std::shared_ptr<InputSendEntry> const& entry) = 0;

    enum InputResponse
    {
        Consumed,
        NotConsumed
    };
    /*!
     * \brief Client responded to an input event.
     *
     * TODO: Only Consumed is supported so far.
     */
    virtual void send_suceeded(std::shared_ptr<InputSendEntry> const& entry, InputResponse response) = 0;

    /*!
     * \brief Called when client is temporarly blocked because input events are still in
     * the queue.
     */
    virtual void client_blocked(std::shared_ptr<InputSendEntry> const& dispatch) = 0;
};

}
}

#endif
