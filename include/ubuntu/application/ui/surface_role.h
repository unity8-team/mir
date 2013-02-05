/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#ifndef UBUNTU_APPLICATION_UI_SURFACE_ROLE_H_
#define UBUNTU_APPLICATION_UI_SURFACE_ROLE_H_

#include "ubuntu/application/ui/ubuntu_application_ui.h"

namespace ubuntu
{
namespace application
{
namespace ui
{
enum SurfaceRole
{
    dash_actor_role = DASH_ACTOR_ROLE,
    main_actor_role = MAIN_ACTOR_ROLE,
    indicator_actor_role = INDICATOR_ACTOR_ROLE, 
    notifications_actor_role = NOTIFICATIONS_ACTOR_ROLE,
    greeter_actor_role = GREETER_ACTOR_ROLE,
    launcher_actor_role = LAUNCHER_ACTOR_ROLE,
    on_screen_keyboard_actor_role = ON_SCREEN_KEYBOARD_ACTOR_ROLE,
    shutdown_dialog_actor_role = SHUTDOWN_DIALOG_ACTOR_ROLE
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SURFACE_ROLE_H_
