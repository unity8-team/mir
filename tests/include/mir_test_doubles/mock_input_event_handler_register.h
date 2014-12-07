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
 * Authored by:
 *   Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_INPUT_MULTIPLEXER_H_
#define MIR_TEST_DOUBLES_MOCK_INPUT_MULTIPLEXER_H_

#include "mir/input/input_event_handler_register.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockInputEventHandlerRegister : public input::InputEventHandlerRegister
{
public:
    void register_fd_handler(
            std::initializer_list<int> fds,
            void const* owner,
            std::function<void(int)> const&& handler) override
    {
        register_fd_handler_(fds, owner, handler);
    }
    MOCK_METHOD3(register_fd_handler_, void(
            std::initializer_list<int> fds,
            void const* owner,
            std::function<void(int)> const& handler));

    MOCK_METHOD1(unregister_fd_handler, void(void const* owner));
    void register_handler(std::function<void()> const&& handler) override
    {
        register_handler_(handler);
    }
    MOCK_METHOD1(register_handler_, void(std::function<void()> const& handler));
};

}
}
}

#endif

