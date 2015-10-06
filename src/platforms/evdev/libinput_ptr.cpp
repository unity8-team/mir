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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "libinput.h"
#include "libinput_ptr.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

namespace mie = mir::input::evdev;

namespace
{
int fd_open(const char* path, int flags, void* /*userdata*/)
{
    return ::open(path, flags);
}

void fd_close(int fd, void* /*userdata*/)
{
    ::close(fd);
}

const libinput_interface fd_ops = {fd_open, fd_close};
}

mie::LibInputPtr mie::make_libinput()
{
    return mie::LibInputPtr{libinput_path_create_context(&fd_ops, nullptr),libinput_unref};
}
