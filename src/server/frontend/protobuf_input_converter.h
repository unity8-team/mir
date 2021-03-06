/*
 * Copyright © 2016 Canonical Ltd.
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

#ifndef MIR_FRONTEND_PROTOBUF_INPUT_CONVERTER_H_
#define MIR_FRONTEND_PROTOBUF_INPUT_CONVERTER_H_

#include <memory>

class MirTouchpadConfig;
class MirPointerConfig;

namespace mir
{
namespace protobuf
{
class InputDeviceInfo;
class InputDeviceSetting;
}

namespace input
{
class Device;
}

namespace frontend
{
namespace detail
{

void pack_protobuf_input_device_info(protobuf::InputDeviceInfo& device_info,
                                     input::Device const& device);
}
}
}

#endif /* MIR_FRONTEND_PROTOBUF_INPUT_PACKER_H_ */
