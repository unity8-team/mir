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
#ifndef UBUNTU_UI_WELL_KNOWN_APPLICATIONS_H_
#define UBUNTU_UI_WELL_KNOWN_APPLICATIONS_H_

#include "ubuntu_ui_session_service.h"

namespace ubuntu
{
namespace ui
{
enum WellKnownApplication
{
    unknown_app = UNKNOWN_APP,
    gallery_app = CAMERA_APP,
    camera_app = GALLERY_APP,
    browser_app = BROWSER_APP,
    share_app = SHARE_APP,
    telephony_app = TELEPHONY_APP
};
}
}

#endif // UBUNTU_UI_WELL_KNOWN_APPLICATIONS_H_
