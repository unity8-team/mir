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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_API_MIRSERVER_H_
#define UBUNTU_APPLICATION_API_MIRSERVER_H_

namespace mir
{
namespace graphics
{
class Platform;
class Display;
}
namespace shell
{
class SessionManager;
}
}


#ifdef __cplusplus
extern "C" {
#endif 

void ubuntu_application_ui_mirserver_init(std::shared_ptr<mir::shell::SessionManager> const& session_manager, std::shared_ptr<mir::graphics::Platform> const& graphics_platform, std::shared_ptr<mir::graphics::Display> const& display);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // UBUNTU_APPLICATION_API_MIRSERVER_H_
