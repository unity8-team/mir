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

#ifndef MIR_GRAPHICS_GBM_SHM_POOL_H_
#define MIR_GRAPHICS_GBM_SHM_POOL_H_

#include <sys/types.h>

namespace mir
{
namespace graphics
{
namespace gbm
{

class ShmPool
{
public:
    virtual ~ShmPool() = default;

    virtual off_t alloc(off_t size) = 0;
    virtual void* map(off_t offset, off_t size) = 0;
    virtual void unmap(void* mapping, off_t size) = 0;
    virtual int fd() const = 0;

protected:
    ShmPool() = default;
    ShmPool(ShmPool const&) = delete;
    ShmPool& operator=(ShmPool const&) = delete;
};

}
}
}

#endif
