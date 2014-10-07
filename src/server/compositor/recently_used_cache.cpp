/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 *              Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/compositor/recently_used_cache.h"
#include "mir/graphics/buffer.h"
#include "mir/geometry/rectangle.h"

namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace geom = mir::geometry;

std::shared_ptr<mg::GLTexture> mc::RecentlyUsedCache::load(mg::Renderable const& renderable)
{
    auto const& buffer = renderable.buffer();
    auto buffer_id = buffer->id();
    auto& texture = textures[renderable.id()];
    auto result = texture.texture;
    result->bind();

    if ((texture.last_bound_buffer != buffer_id) || (!texture.valid_binding))
    {
        auto bound = buffer->gl_bind_to_texture();
        if (bound == mg::Buffer::copied || bound == mg::Buffer::failed)
        {
            texture.resource.reset();
        }
        else if (bound == mg::Buffer::failed)
        {
            texture.resource.reset();
            result.reset();
        }
        else
        {
            texture.resource = buffer;
        }
        texture.last_bound_buffer = buffer_id;
    }
    if (result)
        texture.used = true;
    texture.valid_binding = true;

    return result;
}

void mc::RecentlyUsedCache::invalidate()
{
    for (auto &t : textures)
        t.second.valid_binding = false;
}

void mc::RecentlyUsedCache::drop_unused()
{
    auto t = textures.begin();
    while (t != textures.end())
    {
        auto& tex = t->second;
        tex.resource.reset();
        if (tex.used)
        {
            tex.used = false;
            ++t;
        }
        else
        {
            t = textures.erase(t);
        }
    }
}
