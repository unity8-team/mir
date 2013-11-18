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

#include "shm_pool.h"
#include "shm_buffer.h"
#include "buffer_texture_binder.h"
#include "mir/graphics/buffer_properties.h"

namespace mgg = mir::graphics::gbm;
namespace geom = mir::geometry;

mgg::ShmBuffer::ShmBuffer(
    std::shared_ptr<ShmPool> const& shm_pool,
    BufferProperties const& properties,
    std::unique_ptr<BufferTextureBinder> texture_binder)
    : shm_pool{shm_pool},
      size_{properties.size},
      pixel_format_{properties.format},
      stride_{geom::bytes_per_pixel(pixel_format_) * size_.width.as_uint32_t()},
      texture_binder{std::move(texture_binder)}
{
}

mgg::ShmBuffer::~ShmBuffer() noexcept
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
    auto temp = std::make_shared<MirNativeBuffer>();

    temp->fd_items = 1;
    temp->fd[0] = shm_pool->fd();
    temp->stride = stride().as_uint32_t();
    temp->flags = 0;

    auto const& dim = size();
    temp->width = dim.width.as_int();
    temp->height = dim.height.as_int();

    return temp;
}

bool mgg::ShmBuffer::can_bypass() const
{
    return false;
}
