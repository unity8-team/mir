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
#ifndef UBUNTU_APPLICATION_UI_SESSION_CREDENTIALS_H_
#define UBUNTU_APPLICATION_UI_SESSION_CREDENTIALS_H_

#include "ubuntu_application_ui.h"

#include <cstdio>

namespace ubuntu
{
namespace application
{
namespace ui
{
enum SessionType
{
    user_session_type = USER_SESSION_TYPE,
    system_session_type = SYSTEM_SESSION_TYPE
};

enum MenuBarSupport
{
    application_supports_overlayed_menubar = APPLICATION_SUPPORTS_OVERLAYED_MENUBAR,
    application_does_not_support_overlayed_menubar = APPLICATION_DOES_NOT_SUPPORT_OVERLAYED_MENUBAR
};

class SessionCredentials
{
public:
    SessionCredentials(::SessionCredentials* parent) : parent(parent)
    {
    }

    SessionType session_type() const
    {
        return static_cast<SessionType>(parent->session_type);
    }

    const char* application_name() const
    {
        return parent->application_name;
    }

    
private:
    ::SessionCredentials* parent;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SESSION_CREDENTIALS_H_
