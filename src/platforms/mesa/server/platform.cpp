/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "platform.h"
#include "guest_platform.h"
#include "buffer_allocator.h"
#include "display.h"
#include "linux_virtual_terminal.h"
#include "ipc_operations.h"
#include "mir/graphics/platform_ipc_operations.h"
#include "mir/options/option.h"
#include "mir/graphics/native_buffer.h"
#include "mir/emergency_cleanup_registry.h"


#include <boost/throw_exception.hpp>
#include <stdexcept>

#include <fcntl.h>
#include <sys/ioctl.h>

namespace mg = mir::graphics;
namespace mgm = mg::mesa;
namespace mo = mir::options;

namespace
{
char const* bypass_option_name{"bypass"};
char const* vt_option_name{"vt"};

struct RealVTFileOperations : public mgm::VTFileOperations
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

    int tcsetattr(int d, int acts, const struct termios *tcattr)
    {
        return ::tcsetattr(d, acts, tcattr);
    }

    int tcgetattr(int d, struct termios *tcattr)
    {
        return ::tcgetattr(d, tcattr);
    }
};

struct RealPosixProcessOperations : public mgm::PosixProcessOperations
{
    pid_t getpid() const override
    {
        return ::getpid();
    }
    pid_t getppid() const override
    {
        return ::getppid();
    }
    pid_t getpgid(pid_t process) const override
    {
        return ::getpgid(process);
    }
    pid_t getsid(pid_t process) const override
    {
        return ::getsid(process);
    }
    int setpgid(pid_t process, pid_t group) override
    {
        return ::setpgid(process, group);
    }
    pid_t setsid() override
    {
        return ::setsid();
    }
};

}

mgm::Platform::Platform(std::shared_ptr<DisplayReport> const& listener,
                        std::shared_ptr<VirtualTerminal> const& vt,
                        EmergencyCleanupRegistry& emergency_cleanup_registry,
                        BypassOption bypass_option)
    : udev{std::make_shared<mir::udev::Context>()},
      drm{std::make_shared<helpers::DRMHelper>()},
      listener{listener},
      vt{vt},
      bypass_option_{bypass_option}
{
    drm->setup(udev);
    gbm.setup(*drm);

    std::weak_ptr<VirtualTerminal> weak_vt = vt;
    std::weak_ptr<helpers::DRMHelper> weak_drm = drm;
    emergency_cleanup_registry.add(
        [weak_vt,weak_drm]
        {
            if (auto const vt = weak_vt.lock())
                try { vt->restore(); } catch (...) {}

            if (auto const drm = weak_drm.lock())
                try { drm->drop_master(); } catch (...) {}
        });
}

std::shared_ptr<mg::GraphicBufferAllocator> mgm::Platform::create_buffer_allocator()
{
    return std::make_shared<mgm::BufferAllocator>(gbm.device, bypass_option_);
}

std::shared_ptr<mg::Display> mgm::Platform::create_display(
    std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy,
    std::shared_ptr<GLProgramFactory> const&,
    std::shared_ptr<GLConfig> const& gl_config)
{
    return std::make_shared<mgm::Display>(
        this->shared_from_this(),
        initial_conf_policy,
        gl_config,
        listener);
}

std::shared_ptr<mg::PlatformIpcOperations> mgm::Platform::make_ipc_operations() const
{
    return std::make_shared<mgm::IpcOperations>(drm);
}

EGLNativeDisplayType mgm::Platform::egl_native_display() const
{
    return gbm.device;
}

mgm::BypassOption mgm::Platform::bypass_option() const
{
    return bypass_option_;
}

extern "C" std::shared_ptr<mg::Platform> mg::create_host_platform(
    std::shared_ptr<mo::Option> const& options,
    std::shared_ptr<mir::EmergencyCleanupRegistry> const& emergency_cleanup_registry,
    std::shared_ptr<DisplayReport> const& report)
{
    auto real_fops = std::make_shared<RealVTFileOperations>();
    auto real_pops = std::unique_ptr<RealPosixProcessOperations>(new RealPosixProcessOperations{});
    auto vt = std::make_shared<mgm::LinuxVirtualTerminal>(
        real_fops,
        std::move(real_pops),
        options->get<int>(vt_option_name),
        report);

    auto bypass_option = mgm::BypassOption::allowed;
    if (!options->get<bool>(bypass_option_name))
        bypass_option = mgm::BypassOption::prohibited;

    return std::make_shared<mgm::Platform>(
        report, vt, *emergency_cleanup_registry, bypass_option);
}

extern "C" void add_platform_options(boost::program_options::options_description& config)
{
    config.add_options()
        (vt_option_name,
         boost::program_options::value<int>()->default_value(0),
         "[platform-specific] VT to run on or 0 to use current.")
        (bypass_option_name,
         boost::program_options::value<bool>()->default_value(true),
         "[platform-specific] utilize the bypass optimization for fullscreen surfaces.");
}
