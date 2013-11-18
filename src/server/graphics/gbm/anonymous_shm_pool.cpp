/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by:
 *   Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "anonymous_shm_pool.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>

#include <cstdlib>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>

namespace mgg = mir::graphics::gbm;

namespace
{

int create_anonymous_file()
{
    char tmpl[] = "/tmp/mir-buffer-XXXXXX";

    /* TODO: RAII fd */
    int fd = mkostemp(tmpl, O_CLOEXEC);
    if (fd < 0)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to create temporary file"));
    if (unlink(tmpl) < 0)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to unlink temporary file"));
    return fd;
}

}

mgg::AnonymousShmPool::AnonymousShmPool()
    : fd_(create_anonymous_file())
{
}

off_t mgg::AnonymousShmPool::alloc(off_t size)
{
    if (ftruncate(fd_, size) < 0)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to resize temporary file"));
    return 0;
}

void* mgg::AnonymousShmPool::map(off_t offset, off_t size)
{
    return mmap(nullptr, size, PROT_READ|PROT_WRITE,
                MAP_SHARED, fd_, offset);
}
    
void mgg::AnonymousShmPool::unmap(void* mapping, off_t size)
{
    munmap(mapping, size);
}

int mgg::AnonymousShmPool::fd() const
{
    return fd_;
}
