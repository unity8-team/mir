/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#ifndef MOCK_EVENT_HANDLER_REGISTER_H_
#define MOCK_EVENT_HANDLER_REGISTER_H_

#include "mir/scheduler/event_handler_register.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockEventHandlerRegister : public scheduler::EventHandlerRegister
{
public:
    ~MockEventHandlerRegister() noexcept {}

    MOCK_METHOD2(register_signal_handler,
                 void(std::initializer_list<int>,
                      std::function<void(int)> const&));

    MOCK_METHOD3(register_fd_handler,
                 void(std::initializer_list<int>, void const*,
                      std::function<void(int)> const&));

    MOCK_METHOD1(unregister_fd_handler, void(void const*));
};

}
}
}

#endif
