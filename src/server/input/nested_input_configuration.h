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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_INPUT_NESTED_INPUT_CONFIGURATION_H_
#define MIR_INPUT_NESTED_INPUT_CONFIGURATION_H_

#include "mir/input/input_configuration.h"

#include "mir/cached_ptr.h"

namespace mir
{
namespace input
{
class NestedInputRelay;

class NestedInputConfiguration : public InputConfiguration
{
public:
    NestedInputConfiguration();
    virtual ~NestedInputConfiguration() = default;
    std::shared_ptr<InputChannelFactory> the_input_channel_factory() override;
    std::shared_ptr<InputManager> the_input_manager() override;

private:
    CachedPtr<InputManager> input_manager;
};
}
}

#endif /* MIR_INPUT_NESTED_INPUT_CONFIGURATION_H_ */
