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

#ifndef MIR_TEST_DOUBLES_MOCK_INPUT_MULTIPLEXER_H_
#define MIR_TEST_DOUBLES_MOCK_INPUT_MULTIPLEXER_H_

#include "mir/input/multiplexer.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockMultiplexer : public input::Multiplexer
{
public:
    MOCK_METHOD3(register_fd_handler, void(
            std::initializer_list<int> fds,
            void const* owner,
            std::function<void(int)> const&& handler));

    MOCK_METHOD1(unregister_fd_handler, void(void const* owner));
    MOCK_METHOD1(enqueue_action, void(std::function<void()> const&& action));


};

}
}
}

#endif

