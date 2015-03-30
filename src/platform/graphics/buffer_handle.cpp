/*
 * Copyright © 2015 Canonical Ltd.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "mir/graphics/buffer_handle.h"

namespace mg = mir::graphics;

mg::BufferHandle::BufferHandle(std::shared_ptr<mg::Buffer> const& buffer,
                               ReleaseCallback const& release)
                               : wrapped(buffer),
                                 release_fn(release)
{
}

mg::BufferHandle::BufferHandle(BufferHandle&& other)
{
    *this = std::move(other);
}

mg::BufferHandle& mg::BufferHandle::operator=(BufferHandle&& other)
{
    if (this != &other)
    {
        // If the current buffer is being assigned a null handle,
        // we need to release the buffer.
    	if (other.wrapped && other.release_fn && release_fn)
            release_fn(wrapped.get());
        wrapped = std::move(other.wrapped);
        release_fn.swap(other.release_fn);
        // other is emptied out already, prevent double release
        other.release_fn = nullptr;
    }

    return *this;
}

bool mg::BufferHandle::operator!()
{
    return (wrapped == nullptr);
}

mg::BufferHandle::~BufferHandle()
{
    if (release_fn)
        release_fn(wrapped.get());
}

std::shared_ptr<mg::Buffer> mg::BufferHandle::buffer()
{
    return wrapped;
}
