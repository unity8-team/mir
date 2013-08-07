/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_DISPLAY_CONFIGURATION_POLICY_H_
#define MIR_GRAPHICS_DISPLAY_CONFIGURATION_POLICY_H_

namespace mir
{
namespace graphics
{

class DisplayConfiguration;

class DisplayConfigurationPolicy
{
public:
    virtual ~DisplayConfigurationPolicy() = default;

    virtual void apply_to(DisplayConfiguration& conf) = 0;

protected:
    DisplayConfigurationPolicy() = default;
    DisplayConfigurationPolicy(DisplayConfigurationPolicy const& c) = delete;
    DisplayConfigurationPolicy& operator=(DisplayConfigurationPolicy const& c) = delete;
};

}
}

#endif /* MIR_GRAPHICS_DISPLAY_CONFIGURATION_POLICY_H_ */
