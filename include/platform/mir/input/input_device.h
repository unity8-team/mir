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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_INPUT_INPUT_DEVICE_H_
#define MIR_INPUT_INPUT_DEVICE_H_

#include <memory>

namespace mir
{
namespace input
{
class Multiplexer;
class EventSink;

class InputDevice
{
public:
    InputDevice() = default;
    virtual ~InputDevice() = default;

    virtual void enable_input_events(Multiplexer& trigger_registry, EventSink& destination) = 0;
    virtual void disable_input_events(Multiplexer& trigger_registry) = 0;

    // TODO methods to query device description
protected:
    InputDevice(InputDevice const&) = delete;
    InputDevice& operator=(InputDevice const&) = delete;
};

}
}

#endif

