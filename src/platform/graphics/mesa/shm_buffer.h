/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by:
 *   Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_MESA_SHM_BUFFER_H_
#define MIR_GRAPHICS_MESA_SHM_BUFFER_H_

#include "mir/graphics/buffer_basic.h"
#include "mir/geometry/dimensions.h"
#include "mir/geometry/size.h"
#include "mir_toolkit/common.h"

namespace mir
{
namespace graphics
{
namespace mesa
{

class ShmFile;

class ShmBuffer : public BufferBasic
{
public:
    ShmBuffer(std::shared_ptr<ShmFile> const& shm_file,
              geometry::Size const& size,
              MirPixelFormat const& pixel_format);
    ~ShmBuffer() noexcept;

    geometry::Size size() const;
    geometry::Stride stride() const;
    MirPixelFormat pixel_format() const;
    std::shared_ptr<MirNativeBuffer> native_buffer_handle() const;
    BindResult gl_bind_to_texture() override;
    bool can_bypass() const;

private:
    ShmBuffer(ShmBuffer const&) = delete;
    ShmBuffer& operator=(ShmBuffer const&) = delete;

    std::shared_ptr<ShmFile> const shm_file;
    geometry::Size const size_;
    MirPixelFormat const pixel_format_;
    geometry::Stride const stride_;
    void* const pixels;
};

}
}
}

#endif /* MIR_GRAPHICS_MESA_SHM_BUFFER_H_ */
