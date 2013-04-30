/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "gbm_platform.h"

#include "graphics_buffer/gbm/gbm_buffer_allocator.h"
#include "display/gbm/gbm_display.h"
#include "display/gbm/linux_virtual_terminal.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/graphics/egl/mesa_native_display.h"

#include <xf86drm.h>

#include <fcntl.h>
#include <sys/ioctl.h>

namespace mg = mir::graphics;
namespace mgg = mg::gbm;
namespace mgeglm = mg::egl::mesa;
namespace mc = mir::compositor;

namespace
{

struct GBMPlatformIPCPackage : public mg::PlatformIPCPackage
{
    GBMPlatformIPCPackage(int drm_auth_fd)
    {
        ipc_fds.push_back(drm_auth_fd);
    }

    ~GBMPlatformIPCPackage()
    {
        if (ipc_fds.size() > 0 && ipc_fds[0] >= 0)
            drmClose(ipc_fds[0]);
    }
};

struct RealVTFileOperations : public mgg::VTFileOperations
{
    int open(char const* pathname, int flags)
    {
        return ::open(pathname, flags);
    }

    int close(int fd)
    {
        return ::close(fd);
    }

    int ioctl(int d, int request, int val)
    {
        return ::ioctl(d, request, val);
    }

    int ioctl(int d, int request, void* p_val)
    {
        return ::ioctl(d, request, p_val);
    }
};

}

mgg::GBMPlatform::GBMPlatform(std::shared_ptr<DisplayReport> const& listener,
                              std::shared_ptr<VirtualTerminal> const& vt)
    : listener{listener},
      vt{vt},
      native_display{0}
{
    drm.setup();
    gbm.setup(drm);
}

std::shared_ptr<mc::GraphicBufferAllocator> mgg::GBMPlatform::create_buffer_allocator(
        const std::shared_ptr<mg::BufferInitializer>& buffer_initializer)
{
    return std::make_shared<mgg::GBMBufferAllocator>(this->shared_from_this(),
                                                     buffer_initializer);
}

std::shared_ptr<mg::Display> mgg::GBMPlatform::create_display()
{
    return std::make_shared<mgg::GBMDisplay>(
        this->shared_from_this(),
        listener);
}

std::shared_ptr<mg::PlatformIPCPackage> mgg::GBMPlatform::get_ipc_package()
{
    return std::make_shared<GBMPlatformIPCPackage>(drm.get_authenticated_fd());
}

void mgg::GBMPlatform::drm_auth_magic(drm_magic_t magic)
{
    drm.auth_magic(magic);
}

EGLNativeDisplayType mgg::GBMPlatform::shell_egl_display()
{
    if (native_display)
        return reinterpret_cast<EGLNativeDisplayType>(native_display.get());
    native_display = mgeglm::create_native_display(this->shared_from_this());
    
    return reinterpret_cast<EGLNativeDisplayType>(native_display.get());
}

std::shared_ptr<mg::Platform> mg::create_platform(std::shared_ptr<DisplayReport> const& report)
{
    auto real_fops = std::make_shared<RealVTFileOperations>();
    auto vt = std::make_shared<mgg::LinuxVirtualTerminal>(real_fops, report);
    return std::make_shared<mgg::GBMPlatform>(report, vt);
}
