/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */
#ifndef MIR_FRONTEND_MESSAGE_SENDER_H_
#define MIR_FRONTEND_MESSAGE_SENDER_H_

#include "mir/frontend/fd_sets.h"

#include <sys/types.h>

namespace mir
{
namespace frontend
{
class MessageSender
{
public:
    virtual void send(char const* data, size_t length, FdSets const& fds) = 0;

protected:
    MessageSender() = default;
    virtual ~MessageSender() = default;
    MessageSender(MessageSender const&) = delete;
    MessageSender& operator=(MessageSender const&) = delete;
};

}
}
#endif /* MIR_FRONTEND_MESSAGE_SENDER_H_ */
