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

#ifndef MIR_GRAPHICS_GBM_SHM_BUFFER_H_
#define MIR_GRAPHICS_GBM_SHM_BUFFER_H_

#include "mir/graphics/buffer_basic.h"
#include "mir/geometry/dimensions.h"
#include "mir/geometry/size.h"
#include "mir/geometry/pixel_format.h"

namespace mir
{
namespace graphics
{
namespace gbm
{

namespace detail
{

class ShmPool
{
public:
    ShmPool(off_t size);
    ~ShmPool();

    off_t alloc(off_t size);
    int fd();

private:
    int fd_;
};

}

class ShmBuffer: public BufferBasic
{
public:
    ShmBuffer(BufferProperties const& properties,
              std::unique_ptr<BufferTextureBinder> texture_binder);
    ~ShmBuffer();

    geometry::Size size() const;
    geometry::Stride stride() const;
    geometry::PixelFormat pixel_format() const;
    std::shared_ptr<MirNativeBuffer> native_buffer_handle() const;
    void bind_to_texture();
    bool can_bypass() const;

private:
    ShmBuffer(ShmBuffer const&) = delete;
    ShmBuffer& operator=(ShmBuffer const&) = delete;

    geom::Size const size_;
    geom::PixelFormat const pixel_format_;
    geom::Stride const stride_;
    std::unique_ptr<BufferTextureBinder> texture_binder;
    ShmPool shm_pool;
};

}
}
}

#endif /* MIR_GRAPHICS_GBM_SHM_BUFFER_H_ */
