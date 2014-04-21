/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef MIR_TEST_DOUBLES_STUB_INPUT_SENDER_H_
#define MIR_TEST_DOUBLES_STUB_INPUT_SENDER_H_

#include "mir/input/input_sender.h"
#include "mir/input/input_send_entry.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubInputSender : mir::input::InputSender
{
    void set_send_observer(std::shared_ptr<mir::input::InputSendObserver> const&) override
    {
    }
    std::shared_ptr<mir::input::InputSendEntry> send_event(MirEvent const& ev, mir::input::Surface const&,
                                                           std::shared_ptr<mir::input::InputChannel> const&) override
    {
        return std::make_shared<mir::input::InputSendEntry>(1, ev);
    }
};

}
}
}

#endif
