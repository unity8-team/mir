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

#ifndef MIR_SHELL_DISPLAY_CHANGER_H_
#define MIR_SHELL_DISPLAY_CHANGER_H_

#include <memory>

namespace mir
{
namespace graphics
{
class DisplayConfiguration;
}
namespace frontend
{
class Session;
}

namespace shell
{
class DisplayChanger
{
public:
    virtual ~DisplayChanger() = default;

    virtual std::shared_ptr<graphics::DisplayConfiguration> active_configuration() = 0;
    virtual void configure(std::weak_ptr<frontend::Session> const&, std::shared_ptr<graphics::DisplayConfiguration> const&) = 0;

protected:
    DisplayChanger() = default;
private:
    DisplayChanger(DisplayChanger const&) = delete;
    DisplayChanger& operator=(DisplayChanger const&) = delete;
};

}
}

#endif /* MIR_SHELL_DISPLAY_CHANGER_H_ */
