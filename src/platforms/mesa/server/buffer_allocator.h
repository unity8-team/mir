/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_GRAPHICS_MESA_BUFFER_ALLOCATOR_H_
#define MIR_GRAPHICS_MESA_BUFFER_ALLOCATOR_H_

#include "platform_common.h"
#include "mir/graphics/graphic_buffer_allocator.h"
#include "mir/graphics/buffer_id.h"
#include "mir_toolkit/mir_native_buffer.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic warning "-Wall"
#include <gbm.h>
#pragma GCC diagnostic pop

#include <memory>

namespace mir
{
namespace graphics
{
struct EGLExtensions;

namespace mesa
{

enum class BufferImportMethod
{
    gbm_native_pixmap,
    dma_buf
};

class BufferAllocator: public graphics::GraphicBufferAllocator
{
public:
    BufferAllocator(gbm_device* device, BypassOption bypass_option, BufferImportMethod const buffer_import_method);

    std::shared_ptr<Buffer> alloc_buffer(
        geometry::Size size, uint32_t native_format, uint32_t native_flags) override;
    std::shared_ptr<Buffer> alloc_software_buffer(geometry::Size size, MirPixelFormat) override;
    std::shared_ptr<Buffer> alloc_buffer(graphics::BufferProperties const& buffer_properties) override;
    std::vector<MirPixelFormat> supported_pixel_formats() override;

private:
    std::shared_ptr<Buffer> alloc_hardware_buffer(
        graphics::BufferProperties const& buffer_properties);

    gbm_device* const device;
    std::shared_ptr<EGLExtensions> const egl_extensions;

    BypassOption const bypass_option;
    BufferImportMethod const buffer_import_method;
};

}
}
}

#endif // MIR_GRAPHICS_MESA_BUFFER_ALLOCATOR_H_
