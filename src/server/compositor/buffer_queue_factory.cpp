/*
 * Copyright © 2012, 2015 Canonical Ltd.
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
 *   Alan Griffiths <alan@octopull.co.uk>
 *   Thomas Voss <thomas.voss@canonical.com>
 *   Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "buffer_queue_factory.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/compositor/buffer_queue.h"
#include "mir/graphics/buffer.h"
#include "mir/graphics/buffer_id.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/graphics/display.h"

#include <cassert>
#include <memory>

namespace mc = mir::compositor;
namespace mg = mir::graphics;

mc::BufferQueueFactory::BufferQueueFactory(std::shared_ptr<mg::GraphicBufferAllocator> const& gralloc,
                                           std::shared_ptr<mc::FrameDroppingPolicyFactory> const& policy_factory)
        : gralloc(gralloc),
          policy_factory{policy_factory}
{
    assert(gralloc);
    assert(policy_factory);
}

std::shared_ptr<mc::BufferBundle> mc::BufferQueueFactory::create_buffer_queue(
    mg::BufferProperties const& buffer_properties)
{
    return std::make_shared<mc::BufferQueue>(2, gralloc, buffer_properties, *policy_factory);
}
