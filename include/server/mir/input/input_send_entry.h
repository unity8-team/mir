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

#ifndef MIR_INPUT_INPUT_SEND_ENTRY_H_
#define MIR_INPUT_INPUT_SEND_ENTRY_H_

#include "mir_toolkit/event.h"

namespace mir
{
namespace input
{

/*!
 * Holds information about a sent input event
 * TODO: tobe fleshed out or hidden
 */
class InputSendEntry
{
public:
    uint32_t sequence_id;
    MirEvent event;
    InputSendEntry(uint32_t id, MirEvent ev)
        : sequence_id(id), event(ev)
    {
    }
};
}
}

#endif
