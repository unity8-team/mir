/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "client_buffer.h"
#include "client_buffer_factory.h"
#include "client_buffer_depository.h"

#include <stdexcept>
#include <memory>
#include <map>

namespace mcl=mir::client;

mcl::ClientBufferDepository::ClientBufferDepository(std::shared_ptr<ClientBufferFactory> const& factory, int max_buffers)
    : factory(factory),
      max_buffers(max_buffers)
{
}

void mcl::ClientBufferDepository::deposit_package(std::shared_ptr<MirBufferPackage> const& package, int id, geometry::Size size, geometry::PixelFormat pf)
{
    (void)size; // TODO
    auto existing_buffer_id_pair = buffers.end();
    for (auto pair = buffers.begin(); pair != buffers.end(); ++pair)
    {
        pair->second->increment_age();
        if (pair->first == id)
            existing_buffer_id_pair = pair;
    }

    if (buffers.size() > 0)
        buffers.front().second->mark_as_submitted();

    if (existing_buffer_id_pair == buffers.end())
    {
        auto new_buffer = factory->create_buffer(package, pf);
        buffers.push_front(std::make_pair(id, new_buffer));
    }
    else
    {
        buffers.push_front(*existing_buffer_id_pair);
        buffers.erase(existing_buffer_id_pair);
    }

    if (buffers.size() > max_buffers)
        buffers.pop_back();
}

std::shared_ptr<mcl::ClientBuffer> mcl::ClientBufferDepository::current_buffer()
{
    return buffers.front().second;
}

uint32_t mcl::ClientBufferDepository::current_buffer_id() const
{
    return buffers.front().first;
}
