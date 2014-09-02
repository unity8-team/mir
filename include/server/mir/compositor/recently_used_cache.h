/*
 * Copyright © 2013-2014 Canonical Ltd.
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

#ifndef MIR_COMPOSITOR_RECENTLY_USED_CACHE_H_
#define MIR_COMPOSITOR_RECENTLY_USED_CACHE_H_

#include "mir/graphics/gl_texture_cache.h"
#include "mir/graphics/gl_texture.h"
#include "mir/graphics/buffer_id.h"
#include <unordered_map>

namespace mir
{
namespace compositor 
{
class RecentlyUsedCache : public graphics::GLTextureCache
{
public:
    std::shared_ptr<graphics::GLTexture> load(graphics::Renderable const& renderable) override;
    void invalidate() override;
    void drop_unused() override;

private:
    struct Entry
    {
        Entry()
         : texture(std::make_shared<graphics::GLTexture>())
        {}
        std::shared_ptr<graphics::GLTexture> texture;
        graphics::BufferID last_bound_buffer;
        bool used{true};
        bool valid_binding{false};
        std::shared_ptr<graphics::Buffer> resource;
    };

    std::unordered_map<graphics::Renderable::ID, Entry> textures;
};
}
}

#endif /* MIR_COMPOSITOR_RECENTLY_USED_CACHE_H_ */
