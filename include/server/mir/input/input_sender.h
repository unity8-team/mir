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

#ifndef MIR_INPUT_INPUT_SENDER_H_
#define MIR_INPUT_INPUT_SENDER_H_

#include "mir_toolkit/event.h"

#include <memory>

namespace mir
{
namespace input
{
class InputSendObserver;
class InputSendEntry;
class Surface;
class InputChannel;

class InputSender
{
public:
    virtual ~InputSender() = default;
    virtual void set_send_observer(std::shared_ptr<InputSendObserver> const& observer) = 0;
    virtual std::shared_ptr<InputSendEntry> send_event(MirEvent const& event, std::shared_ptr<Surface> const& receiver, std::shared_ptr<InputChannel> const& channel) = 0;
};

}
}

#endif
