/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_INPUT_INPUT_DEVICE_H_
#define MIR_INPUT_INPUT_DEVICE_H_

#include "mir/udev/wrapper.h"

#include <memory>

namespace mir
{
namespace input
{

class InputDevice
{
public:
    // TODO
    InputDevice(mir::udev::Device const& device) { (void)device; }
};

}
} // namespace mir

#endif // MIR_INPUT_INPUT_DEVICE_H_


