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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_SHELL_MEDIATING_DISPLAY_CHANGER_H_
#define MIR_SHELL_MEDIATING_DISPLAY_CHANGER_H_

#include "mir/shell/display_changer.h"

namespace mir
{

namespace graphics
{
class Display;
}
namespace shell
{

class MediatingDisplayChanger : public shell::DisplayChanger 
{
public:
    explicit MediatingDisplayChanger(std::shared_ptr<graphics::Display> const& changer);

    std::shared_ptr<graphics::DisplayConfiguration> active_configuration();
    void configure(std::weak_ptr<frontend::Session> const&, std::shared_ptr<graphics::DisplayConfiguration> const&);

private:
    std::shared_ptr<graphics::Display> const display;
};

}
}

#endif /* MIR_SHELL_MEDIATING_DISPLAY_CHANGER_H_ */
