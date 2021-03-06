/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "buffer_acquisition.h"
#include "temporary_buffers.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mc=mir::compositor;
namespace mg=mir::graphics;
namespace geom=mir::geometry;

mc::TemporaryBuffer::TemporaryBuffer(std::shared_ptr<mg::Buffer> const& real_buffer)
    : buffer(real_buffer)
{
}

mc::TemporaryCompositorBuffer::TemporaryCompositorBuffer(
    std::shared_ptr<BufferAcquisition> const& acquisition, void const* user_id)
    : TemporaryBuffer(acquisition->compositor_acquire(user_id)),
      acquisition(acquisition)
{
}

mc::TemporaryCompositorBuffer::~TemporaryCompositorBuffer()
{
    acquisition->compositor_release(buffer);
}

mc::TemporarySnapshotBuffer::TemporarySnapshotBuffer(
    std::shared_ptr<BufferAcquisition> const& acquisition)
    : TemporaryBuffer(acquisition->snapshot_acquire()),
      acquisition(acquisition)
{
}

mc::TemporarySnapshotBuffer::~TemporarySnapshotBuffer()
{
    acquisition->snapshot_release(buffer);
}

geom::Size mc::TemporaryBuffer::size() const
{
    return buffer->size();
}

MirPixelFormat mc::TemporaryBuffer::pixel_format() const
{
    return buffer->pixel_format();
}

mg::BufferID mc::TemporaryBuffer::id() const
{
    return buffer->id();
}

std::shared_ptr<mg::NativeBuffer> mc::TemporaryBuffer::native_buffer_handle() const
{
    return buffer->native_buffer_handle();
}

mg::NativeBufferBase* mc::TemporaryBuffer::native_buffer_base()
{
    return buffer->native_buffer_base();
}
