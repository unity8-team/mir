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

#include "buffer_object.h"
#include <xf86drmMode.h>

namespace mgm = mir::graphics::mesa;

mgm::BufferObject::BufferObject(gbm_surface* surface, gbm_bo* bo, uint32_t drm_fb_id) :
    surface{surface}, bo{bo}, drm_fb_id{drm_fb_id}
{
}

mgm::BufferObject::~BufferObject()
{
    if (drm_fb_id)
    {
        int drm_fd = gbm_device_get_fd(gbm_bo_get_device(bo));
        drmModeRmFB(drm_fd, drm_fb_id);
    }
}

void mgm::BufferObject::release() const
{
    gbm_surface_release_buffer(surface, bo);
}

uint32_t mgm::BufferObject::get_drm_fb_id() const
{
    return drm_fb_id;
}
