/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#define MIR_LOG_COMPONENT "x11-error"
#include "mir/log.h"

#include "graphics/platform.h"
#include "X11_resources.h"

namespace mx = mir::X;

//Force synchronous Xlib operation - for debugging
//#define FORCE_SYNCHRONOUS

std::shared_ptr<void> get_module_context()
{
    static std::shared_ptr<mx::X11Resources> anchor;

    if (!anchor)
        anchor = std::make_shared<mx::X11Resources>();

    return anchor;
}

int mx::mir_x11_error_handler(Display* dpy, XErrorEvent* eev)
{
    char msg[80];
    XGetErrorText(dpy, eev->error_code, msg, sizeof(msg));
    log_error("X11 error %d (%s): request %d.%d\n",
        eev->error_code, msg, eev->request_code, eev->minor_code);
    return 0;
}

std::shared_ptr<::Display> mx::X11Resources::get_conn()
{
    if (auto conn = connection.lock())
        return conn;

    XInitThreads();

    XSetErrorHandler(mir_x11_error_handler);

    std::shared_ptr<::Display> new_conn{
        XOpenDisplay(nullptr),
        [](::Display* display) { XCloseDisplay(display); }};

#ifdef FORCE_SYNCHRONOUS
    XSynchronize(new_conn.get(), True);
#endif
    connection = new_conn;
    return new_conn;
}
