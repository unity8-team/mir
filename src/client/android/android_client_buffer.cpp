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
 * Authored by: Kevin DuBois<kevin.dubois@canonical.com>
 */

#include "mir/graphics/android/mir_native_buffer.h"
#include "mir_toolkit/mir_client_library.h"
#include "android_client_buffer.h"
#include <hardware/gralloc.h>

namespace mcl=mir::client;
namespace mcla=mir::client::android;
namespace geom=mir::geometry;
namespace mga=mir::graphics::android;

mcla::AndroidClientBuffer::AndroidClientBuffer(std::shared_ptr<AndroidRegistrar> const& registrar,
                                               std::shared_ptr<const native_handle_t> const& handle,
                                               geom::Size size, geom::PixelFormat pf, geometry::Stride stride)
 : buffer_registrar(registrar),
   native_handle(handle),
   buffer_pf(pf), buffer_stride{stride}
{
    auto tmp = new mga::MirNativeBuffer(handle);
    native_window_buffer = std::shared_ptr<mga::MirNativeBuffer>(tmp, [](mga::MirNativeBuffer* buffer)
        {
            buffer->mir_dereference();
        });

    native_window_buffer->height = static_cast<int32_t>(size.height.as_uint32_t());
    native_window_buffer->width =  static_cast<int32_t>(size.width.as_uint32_t());
    native_window_buffer->stride = stride.as_uint32_t() /
                                   geom::bytes_per_pixel(buffer_pf);
    native_window_buffer->usage = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER;
    native_window_buffer->handle = native_handle.get();
}

mcla::AndroidClientBuffer::~AndroidClientBuffer() noexcept
{
}

std::shared_ptr<mcl::MemoryRegion> mcla::AndroidClientBuffer::secure_for_cpu_write()
{
    auto rect = geom::Rectangle{geom::Point{0, 0}, size()};
    auto vaddr = buffer_registrar->secure_for_cpu(native_handle, rect);
    auto region =  std::make_shared<mcl::MemoryRegion>();
    region->vaddr = vaddr;
    region->width = rect.size.width;
    region->height = rect.size.height;
    region->stride = stride();
    region->format = buffer_pf;
    return region;
}

geom::Size mcla::AndroidClientBuffer::size() const
{
    return {native_window_buffer->width, native_window_buffer->height};
}

geom::Stride mcla::AndroidClientBuffer::stride() const
{
    return buffer_stride;
}

geom::PixelFormat mcla::AndroidClientBuffer::pixel_format() const
{
    return buffer_pf;
}

std::shared_ptr<ANativeWindowBuffer> mcla::AndroidClientBuffer::native_buffer_handle() const
{
    return native_window_buffer;
}
