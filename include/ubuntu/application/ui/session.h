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
#ifndef UBUNTU_APPLICATION_UI_SESSION_H_
#define UBUNTU_APPLICATION_UI_SESSION_H_

#include "ubuntu/application/session.h"
#include "ubuntu/application/ui/clipboard.h"
#include "ubuntu/application/ui/physical_display_info.h"
#include "ubuntu/application/ui/surface.h"
#include "ubuntu/application/ui/surface_properties.h"
#include "ubuntu/platform/shared_ptr.h"

#include <EGL/egl.h>

namespace ubuntu
{
namespace application
{
namespace ui
{
/**
 * Represents a session with the service providers abstracted by
 * Ubuntu platform API and allows for accessing ui-specific
 * functionality.
 */
class Session : public ubuntu::application::Session
{
public:
    typedef ubuntu::platform::shared_ptr<Session> Ptr;

    /** Provides access to the system-wide clipboard. */
    static Clipboard::Ptr clipboard();

    /** Provides system-wide access to the physical displays known to the system. 
     *  \sa PhysicalDisplayIdentifier
     *  \sa PhysicalDisplayInfo
     */
    static PhysicalDisplayInfo::Ptr physical_display_info(PhysicalDisplayIdentifier id);

    /** Requests a surface from the system.
     *  \param [in] props Requested properties for the surface
     *  \param [in] listener Event receiver for input events
     *  \return Pointer to the surface instance or NULL.
     */
    virtual Surface::Ptr create_surface(
        const SurfaceProperties& props,
        const ubuntu::application::ui::input::Listener::Ptr& listener) = 0;

    /** Requests the PID for the running session. */ 
    virtual int get_session_pid() = 0;

    /** Destroys the surface and renders it unusable. */
    virtual void destroy_surface(const Surface::Ptr& surface) = 0;

    /** Requests the surface to be shown in fullscreen. 
     *  \param [in] surface The surface to transition to fullscreen.
     */
    virtual void toggle_fullscreen_for_surface(const Surface::Ptr& surface) = 0;

protected:
    Session() {}
    virtual ~Session() {}

    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
};
}
}
}

#endif // UBUNTU_APPLICATION_UI_SESSION_H_
