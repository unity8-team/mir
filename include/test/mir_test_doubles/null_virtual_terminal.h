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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_NULL_VIRTUAL_TERMINAL_H_
#define MIR_TEST_DOUBLES_NULL_VIRTUAL_TERMINAL_H_

#include "src/server/graphics/gbm/virtual_terminal.h"

namespace mir
{
namespace test
{
namespace doubles
{

class NullVirtualTerminal : public graphics::gbm::VirtualTerminal
{
public:
    void set_graphics_mode() {}
    void disable_control_sequences() {}

    void register_switch_handlers(MainLoop&,
                                  std::function<bool()> const&,
                                  std::function<bool()> const&)
    {
    }
};

}
}
}

#endif /* MIR_TEST_DOUBLES_NULL_VIRTUAL_TERMINAL_H_ */
