/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/android/sync_fence.h"

#include <unistd.h>
#include <sys/ioctl.h>
#include <android/sync/sync.h>
//#include <android/linux/sync.h>

namespace mga = mir::graphics::android;

mga::SyncFence::SyncFence(std::shared_ptr<mga::SyncFileOps> const& ops, Fd fd)
   : fence_fd(std::move(fd)),
     ops(ops)
{
}

void mga::SyncFence::wait()
{
    if (fence_fd > 0)
    {
        int timeout = infinite_timeout;
        ops->wait(fence_fd, timeout);
        fence_fd = mir::Fd(Fd::invalid);
    }
}

void mga::SyncFence::merge_with(NativeFence& merge_fd)
{
    if (merge_fd < 0)
    {
        return;
    }

    if (fence_fd < 0)
    {
        //our fence was invalid, adopt the other fence
        fence_fd = mir::Fd(merge_fd);
    }
    else
    {
        //both fences were valid, must merge
        fence_fd = mir::Fd(ops->merge("mirfence", fence_fd, merge_fd));
        ops->close(merge_fd);
    }

    merge_fd = -1;
}

mga::NativeFence mga::SyncFence::copy_native_handle() const
{
    return ops->dup(fence_fd);
}

int mga::RealSyncFileOps::merge(char const* name, int fd1, int fd2)
{
    return ::sync_merge(name, fd1, fd2);
}

int mga::RealSyncFileOps::wait(int fd, int timeout)
{
    return ::sync_wait(fd, timeout);
}

int mga::RealSyncFileOps::dup(int fd)
{
    return ::dup(fd);
}

int mga::RealSyncFileOps::close(int fd)
{
    return ::close(fd);
}
