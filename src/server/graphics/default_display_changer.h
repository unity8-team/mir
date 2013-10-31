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

#ifndef MIR_GRAPHICS_DEFAULT_DISPLAY_CHANGER_H_
#define MIR_GRAPHICS_DEFAULT_DISPLAY_CHANGER_H_

#include "mir/graphics/display_changer.h"

namespace mir
{
namespace compositor
{
class Compositor;
}
namespace graphics
{
class Display;

class DefaultDisplayChanger : public DisplayChanger
{
public:
    DefaultDisplayChanger(std::shared_ptr<Display> const& display, std::shared_ptr<compositor::Compositor> const& compositor);
    virtual ~DefaultDisplayChanger() = default;

    std::shared_ptr<DisplayConfiguration> configuration() override;
    void configure(std::shared_ptr<DisplayConfiguration> const& new_conf) override;
private:
    std::shared_ptr<Display> const display;
    std::shared_ptr<compositor::Compositor> const compositor;
};

}
}

#endif /* MIR_GRAPHICS_DEFAULT_DISPLAY_CHANGER_H_ */
