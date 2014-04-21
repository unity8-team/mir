/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_MAIN_LOOP_H_
#define MIR_TEST_DOUBLES_MOCK_MAIN_LOOP_H_

#include "mir/main_loop.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{
class MockMainLoop : public mir::MainLoop
{
public:
    ~MockMainLoop() noexcept {}

    void run() override {}
    void stop() override {}

    MOCK_METHOD2(register_signal_handler,
                 void(std::initializer_list<int>,
                      std::function<void(int)> const&));

    MOCK_METHOD2(register_fd_handler,
                 void(std::initializer_list<int>,
                      std::function<void(int)> const&));
};

}
}
}

#endif
