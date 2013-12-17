/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef MIR_DEMO_SHELL_CASCADED_DISPLAY_CONFIGURATION_POLICY_H
#define MIR_DEMO_SHELL_CASCADED_DISPLAY_CONFIGURATION_POLICY_H

#include "mir/graphics/display_configuration_policy.h"
#include <memory>

namespace mir
{
namespace examples
{

class CascadedDisplayConfigurationPolicy : public graphics::DisplayConfigurationPolicy
{
public:
    CascadedDisplayConfigurationPolicy(std::shared_ptr<graphics::DisplayConfigurationPolicy> const& left,
                                       std::shared_ptr<graphics::DisplayConfigurationPolicy> const& right);
    virtual void apply_to(graphics::DisplayConfiguration& conf);
private:
    std::shared_ptr<DisplayConfigurationPolicy> left, right;
};

}
}
#endif

