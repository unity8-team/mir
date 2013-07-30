/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "internal_native_display.h"
#include "mir/graphics/platform_ipc_package.h"

#include "mir_toolkit/mesa/native_display.h"

namespace mg = mir::graphics;
namespace mgg = mir::graphics::gbm;

mgg::InternalNativeDisplay::InternalNativeDisplay(std::shared_ptr<mg::PlatformIPCPackage> const& platform_package)
    : platform_package(platform_package)
{
    context = this;
    this->display_get_platform = &InternalNativeDisplay::native_display_get_platform;
}

int mgg::InternalNativeDisplay::native_display_get_platform(MirMesaEGLNativeDisplay* display, MirPlatformPackage* package)
{
    auto native_disp = static_cast<InternalNativeDisplay*>(display);
    package->data_items = native_disp->platform_package->ipc_data.size();
    for (int i = 0; i < package->data_items; i++)
    {
        package->data[i] = native_disp->platform_package->ipc_data[i];
    }
    package->fd_items = native_disp->platform_package->ipc_fds.size();
    for (int i = 0; i < package->fd_items; i++)
    {
        package->fd[i] = native_disp->platform_package->ipc_fds[i];
    }

    return MIR_MESA_TRUE;
}
