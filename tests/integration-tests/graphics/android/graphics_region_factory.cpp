/*
 * Copyright © 2012, 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "graphics_region_factory.h"
#include "mir_toolkit/mir_client_library.h"
#include "mir/graphics/android/native_buffer.h"
#include "mir_toolkit/common.h"
#include <stdexcept>

namespace mga = mir::graphics::android;
namespace mt = mir::test;

mt::GraphicsRegionFactory::GraphicsRegionFactory()
{
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, reinterpret_cast<hw_module_t const**>(&module)))
        throw std::runtime_error("error, hw module not available!\n");
}

std::shared_ptr<MirGraphicsRegion> mt::GraphicsRegionFactory::graphic_region_from_handle(
    mir::graphics::NativeBuffer& native_buffer)
{
    native_buffer.ensure_available_for(mga::BufferAccess::write);
    auto anwb = native_buffer.anwb();
    void* vaddr{nullptr};
    module->lock(module, anwb->handle, usage, 0, 0, anwb->width, anwb->height, &vaddr);

    auto* region = new MirGraphicsRegion;
    region->vaddr = static_cast<char*>(vaddr);
    region->stride = anwb->stride * MIR_BYTES_PER_PIXEL(mir_pixel_format_abgr_8888);
    region->width = anwb->width;
    region->height = anwb->height;
    region->pixel_format = mir_pixel_format_abgr_8888;

    return std::shared_ptr<MirGraphicsRegion>(region,
        [this, anwb](MirGraphicsRegion* region)
        {
            module->unlock(module, anwb->handle);
            delete region;
        });
}
