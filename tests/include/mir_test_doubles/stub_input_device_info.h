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

#ifndef MIR_TEST_DOUBLES_STUB_INPUT_INPUT_DEVICE_INFO_H_
#define MIR_TEST_DOUBLES_STUB_INPUT_INPUT_DEVICE_INFO_H_

#include "src/server/input/input_device_info.h"

namespace mir
{
namespace test
{
namespace doubles
{

class StubInputDeviceInfo : public mir::input::InputDeviceInfo
{
public:
    explicit StubInputDeviceInfo(uint32_t classes = 0)
        : input_classes(classes)
    {}
    std::string path() const override
    {
        return "";
    }
    uint32_t device_classes() const override
    {
        return input_classes;
    }
    mir::input::InputDeviceIdentifier id() const override
    {
        return mir::input::InputDeviceIdentifier();
    }
    uint32_t input_classes;
};

}
}
}

#endif
