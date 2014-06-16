/*
 * Copyright © 2013 Canonical Ltd.
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

#ifndef MIR_GRAPHICS_MESA_LINUX_VIRTUAL_TERMINAL_H_
#define MIR_GRAPHICS_MESA_LINUX_VIRTUAL_TERMINAL_H_

#include "virtual_terminal.h"

#include <memory>

#include <linux/vt.h>
#include <termios.h>
#include <unistd.h>

namespace mir
{
namespace graphics
{

class DisplayReport;

namespace mesa
{

class VTFileOperations
{
public:
    virtual ~VTFileOperations() = default;

    virtual int open(char const* pathname, int flags) = 0;
    virtual int close(int fd) = 0;
    virtual int ioctl(int d, int request, int val) = 0;
    virtual int ioctl(int d, int request, void* p_val) = 0;
    virtual int tcsetattr(int d, int acts, const struct termios *tcattr) = 0;
    virtual int tcgetattr(int d, struct termios *tcattr) = 0;

protected:
    VTFileOperations() = default;
    VTFileOperations(VTFileOperations const&) = delete;
    VTFileOperations& operator=(VTFileOperations const&) = delete;
};

class PosixProcessOperations
{
public:
    virtual ~PosixProcessOperations() = default;

    virtual pid_t getpid() const = 0;
    virtual pid_t getppid() const = 0;
    virtual pid_t getpgid(pid_t process) const = 0;
    virtual pid_t getsid(pid_t process) const = 0;

    virtual int setpgid(pid_t process, pid_t group) = 0;
    virtual pid_t setsid() = 0;

protected:
    PosixProcessOperations() = default;
    PosixProcessOperations(PosixProcessOperations const&) = delete;
    PosixProcessOperations& operator=(PosixProcessOperations const&) = delete;
};

class LinuxVirtualTerminal : public VirtualTerminal
{
public:
    LinuxVirtualTerminal(std::shared_ptr<VTFileOperations> const& fops,
                         std::unique_ptr<PosixProcessOperations> pops,
                         int vt_number,
                         std::shared_ptr<DisplayReport> const& report);
    ~LinuxVirtualTerminal() noexcept(true);

    void set_graphics_mode() override;
    void register_switch_handlers(
        EventHandlerRegister& handlers,
        std::function<bool()> const& switch_away,
        std::function<bool()> const& switch_back) override;
    void restore() override;

private:
    class FDWrapper
    {
    public:
        FDWrapper(std::shared_ptr<VTFileOperations> const& fops, int fd)
            : fops{fops}, fd_{fd}
        {
        }
        ~FDWrapper() { if (fd_ >= 0) fops->close(fd_); }
        int fd() const { return fd_; }
    private:
        std::shared_ptr<VTFileOperations> const fops;
        int const fd_;
    };

    int find_active_vt_number();
    int open_vt(int vt_number);


    std::shared_ptr<VTFileOperations> const fops;
    std::unique_ptr<PosixProcessOperations> const pops;
    std::shared_ptr<DisplayReport> const report;
    FDWrapper const vt_fd;
    int prev_kd_mode;
    struct vt_mode prev_vt_mode;
    int prev_tty_mode;
    struct termios prev_tcattr;
    bool active;
};

}
}
}

#endif /* MIR_GRAPHICS_MESA_LINUX_VIRTUAL_TERMINAL_H_ */
