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

#ifndef MIR_GRAPHICS_DISPLAY_CHANGER_H_
#define MIR_GRAPHICS_DISPLAY_CHANGER_H_

#include "mir/graphics/display_changer.h"

#include <memory>

namespace mir
{
namespace graphics
{
class DisplayConfiguration;

class DisplayChanger
{
public:
    virtual ~DisplayChanger() = default;

    virtual std::shared_ptr<DisplayConfiguration> configuration() = 0;
    virtual void configure(std::shared_ptr<DisplayConfiguration> const& new_conf) = 0;

protected:
    DisplayChanger() = default;
};

}
}

#endif /* MIR_GRAPHICS_DISPLAY_CHANGER_H_ */
