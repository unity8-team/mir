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

#include "buffer_handle.h"
#include "buffer_bundle.h"

namespace mc = mir::compositor;
namespace mg = mir::graphics;

mc::BufferHandle::BufferHandle(BufferBundle* bundle,
                               std::shared_ptr<mg::Buffer> const& buffer,
                               release_callback const& release)
                               : buffer_bundle(bundle),
                                 wrapped(buffer),
                                 release_fn(release)
{
}

mc::BufferHandle::~BufferHandle()
{
    if (release_fn)
    	release_fn(wrapped.get());
}

std::shared_ptr<mg::Buffer> mc::BufferHandle::buffer()
{
    return wrapped;
}
