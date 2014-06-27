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

#ifndef MIR_TEST_DOUBLES_STUB_INPUT_SENDER_H_
#define MIR_TEST_DOUBLES_STUB_INPUT_SENDER_H_

#include "mir/input/input_sender.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubInputSender : mir::input::InputSender
{
    void send_event(MirEvent const& /*event*/, std::shared_ptr<mir::input::InputChannel> const& /*channel*/) override
    {
    }
};

}
}
}

#endif
