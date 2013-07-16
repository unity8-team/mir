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
 * Authored by:
 *   Alan Griffiths <alan@octopull.co.uk>
 *   Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/compositor/buffer_allocation_strategy.h"
#include "mir/compositor/buffer_stream_factory.h"
#include "mir/compositor/buffer_stream_surfaces.h"
#include "mir/compositor/buffer_properties.h"
#include "switching_bundle.h"
#include "mir/graphics/buffer.h"
#include "mir/graphics/buffer_id.h"
#include "mir/compositor/graphic_buffer_allocator.h"
#include "mir/graphics/display.h"

#include <cassert>
#include <memory>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace ms = mir::surfaces;

mc::BufferStreamFactory::BufferStreamFactory(
    const std::shared_ptr<BufferAllocationStrategy>& strategy)
        : swapper_factory(strategy)
{
    assert(strategy);
}


std::shared_ptr<ms::BufferStream> mc::BufferStreamFactory::create_buffer_stream(
    mc::BufferProperties const& buffer_properties)
{
    auto switching_bundle = std::make_shared<mc::SwitchingBundle>(swapper_factory, buffer_properties);
    return std::make_shared<mc::BufferStreamSurfaces>(switching_bundle);
}
