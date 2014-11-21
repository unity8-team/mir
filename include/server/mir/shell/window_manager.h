/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_SHELL_WINDOW_MANAGER_H_
#define MIR_SHELL_WINDOW_MANAGER_H_

namespace mir
{
namespace scene
{
    class Session;
    struct SurfaceCreationParameters;
}

namespace shell
{

class WindowManager
{
public:
    virtual ~WindowManager() = default;

    virtual scene::SurfaceCreationParameters place(scene::Session const&,
                                 scene::SurfaceCreationParameters const&) = 0;

protected:
    WindowManager() = default;
    WindowManager(WindowManager const&) = delete;
    WindowManager& operator=(WindowManager const&) = delete;
};
}
} // namespace mir

#endif // MIR_SHELL_WINDOW_MANAGER_H_