/*
 * Copyright (C) 2013 Canonical Ltd
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef UBUNTU_APPLICATION_API_MIRSERVER_PRIV_H_
#define UBUNTU_APPLICATION_API_MIRSERVER_PRIV_H_

#include <ubuntu/visibility.h>

namespace mir
{
class DefaultServerConfiguration;
}

// We need to provide the platform API with Mir server objects as dependency. The analog
// on the client side is of course the named socket for connection to Mir. This should be
// invoked from the run_mir handler prior to creation of a UApplicationInstance or usage of
// any ua_ui functions.

extern "C"
{
UBUNTU_DLL_PUBLIC void ua_ui_mirserver_init(mir::DefaultServerConfiguration& config);

// Release platform-api ownership of Mir server objects to allow for clean shutdown.
UBUNTU_DLL_PUBLIC void ua_ui_mirserver_finish();
}

#endif // UBUNTU_APPLICATION_API_MIRSERVER_PRIV_H_

