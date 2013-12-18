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

#include "cascaded_display_configuration_policy.h"

namespace mir
{
namespace examples
{

CascadedDisplayConfigurationPolicy::CascadedDisplayConfigurationPolicy(
        std::shared_ptr<graphics::DisplayConfigurationPolicy> const& l,
        std::shared_ptr<graphics::DisplayConfigurationPolicy> const& r)
    : left(l),
    right(r) 
{
}

void CascadedDisplayConfigurationPolicy::apply_to(graphics::DisplayConfiguration& conf)
{
    left->apply_to(conf);
    right->apply_to(conf);
}

}
}

