/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
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
/**
 * Describes the role of a surface.
 * \attention Reserved roles require special privileges.
 */
enum SurfaceRole
{
    main_actor_role = MAIN_ACTOR_ROLE, ///< An application's main surface

    dash_actor_role = DASH_ACTOR_ROLE, ///< Reserved for the shell's dash
    indicator_actor_role = INDICATOR_ACTOR_ROLE, ///< Reserved for the shell's indicators
    notifications_actor_role = NOTIFICATIONS_ACTOR_ROLE, ///< Reserved for the shell's notifications
    greeter_actor_role = GREETER_ACTOR_ROLE, ///< Reserved for the greeter
    launcher_actor_role = LAUNCHER_ACTOR_ROLE, ///< Reserved for the launcher
    on_screen_keyboard_actor_role = ON_SCREEN_KEYBOARD_ACTOR_ROLE, ///< Reserved for the onscreen-keyboard
    shutdown_dialog_actor_role = SHUTDOWN_DIALOG_ACTOR_ROLE ///< Reserved for the shutdown dialog
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SURFACE_ROLE_H_
