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

#include "shm_buffer.h"
#include "mir/graphics/buffer_properties.h"

#include <cstdlib>

namespace mgg = mir::graphics::gbm;
namespace geom = mir::geometry;

namespace
{

int create_anonymous_file(off_t length)
{
    int fd = mkostemp("/tmp/mir-buffer-XXXXXX", O_CLOEXEC);
    ftruncate(fd, length);
    return fd;
}

}

mgg::detail::ShmPool::ShmPool(off_t size)
    : fd_(create_anonymous_file(size))
{
}

mgg::detail::ShmPool::~ShmPool()
{
    close(fd_);
}

off_t mgg::detail::ShmPool::alloc(off_t size)
{
    return 0;
}

int mgg::detail::ShmPool::fd()
{
    return fd_;
}

mgg::ShmBuffer:ShmBuffer(
    BufferProperties const& properties,
    std::unique_ptr<BufferTextureBinder> texture_binder)
    : size_{properties.size},
      pixel_format_{properties.pixel_format},
      stride{geom::bytes_per_pixel(pixel_format_) * size_.width},
      texture_binder{texture_binder}
{
}

mgg::ShmBuffer::~ShmBuffer()
{
}

geom::Size mgg::ShmBuffer::size() const
{
    return size_;
}

geom::Stride mgg::ShmBuffer::stride() const
{
    return stride_;
}

geom::PixelFormat mgg::ShmBuffer::pixel_format() const
{
    return pixel_format_;
}

void mgg::ShmBuffer::bind_to_texture()
{
    texture_binder->bind_to_texture();
}

std::shared_ptr<MirNativeBuffer> mgg::ShmBuffer::native_buffer_handle() const
{
    auto temp = std::make_shared<ShmNativeBuffer>();

    temp->fd_items = 1;
    temp->fd[0] = pool.fd();
    temp->stride = stride().as_uint32_t();
    temp->flags = 0
    temp->bo = nullptr;

    auto const& dim = size();
    temp->width = dim.width.as_int();
    temp->height = dim.height.as_int();

    return temp;
}

bool mgg::ShmBuffer::can_bypass() const
{
    return bo_flags & Shm_BO_USE_SCANOUT;
}
