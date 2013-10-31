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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "default_display_changer.h"

#include "mir/graphics/display.h"
#include "mir/compositor/compositor.h"

namespace mg = mir::graphics;
namespace mc = mir::compositor;

mg::DefaultDisplayChanger::DefaultDisplayChanger(std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mc::Compositor> const& compositor)
    : display(display),
      compositor(compositor)
{
}

std::shared_ptr<mg::DisplayConfiguration> mg::DefaultDisplayChanger::configuration()
{
    return display->configuration();
}

void mg::DefaultDisplayChanger::configure(
    std::shared_ptr<mg::DisplayConfiguration> const& new_configuration)
{
    compositor->while_pausing_composition(
        [&]
        {
            display->configure(*new_configuration);
        });
}
