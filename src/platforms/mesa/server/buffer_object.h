/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_MESA_BUFFER_OBJECT_H_
#define MIR_GRAPHICS_MESA_BUFFER_OBJECT_H_

#include <stddef.h>
#include <gbm.h>

namespace mir
{
namespace graphics
{
namespace mesa
{
class BufferObject
{
public:
    BufferObject(gbm_surface* surface, gbm_bo* bo, uint32_t drm_fb_id);
    ~BufferObject();

    void release() const;
    uint32_t get_drm_fb_id() const;
private:
    BufferObject(BufferObject const&) = delete;
    BufferObject& operator=(BufferObject const&) = delete;

    gbm_surface *surface;
    gbm_bo *bo;
    uint32_t drm_fb_id;
};
}
}
}

#endif /* MIR_GRAPHICS_MESA_BUFFER_OBJECT_H_ */
