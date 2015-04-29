
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
 * Authored by:
 *   Cemil Azizoglu <cemil.azizoglu@canonical.com>
 *
 */

#include "gbm_buffer.h"
#include "buffer_texture_binder.h"

#include <fcntl.h>
#include <xf86drm.h>

#include <boost/exception/errinfo_errno.hpp>
#include <boost/throw_exception.hpp>

#include <stdexcept>

namespace mgx=mir::graphics::X;
namespace geom=mir::geometry;

MirPixelFormat mgx::gbm_format_to_mir_format(uint32_t format)
{
    MirPixelFormat pf;

    switch (format)
    {
    case GBM_BO_FORMAT_ARGB8888:
    case GBM_FORMAT_ARGB8888:
        pf = mir_pixel_format_argb_8888;
        break;
    case GBM_BO_FORMAT_XRGB8888:
    case GBM_FORMAT_XRGB8888:
        pf = mir_pixel_format_xrgb_8888;
        break;
    default:
        pf = mir_pixel_format_invalid;
        break;
    }

    return pf;
}

uint32_t mgx::mir_format_to_gbm_format(MirPixelFormat format)
{
    uint32_t gbm_pf;

    switch (format)
    {
    case mir_pixel_format_argb_8888:
        gbm_pf = GBM_FORMAT_ARGB8888;
        break;
    case mir_pixel_format_xrgb_8888:
        gbm_pf = GBM_FORMAT_XRGB8888;
        break;
    default:
        gbm_pf = mgx::invalid_gbm_format;
        break;
    }

    return gbm_pf;
}

mgx::GBMBuffer::GBMBuffer(std::shared_ptr<gbm_bo> const& handle,
                          uint32_t bo_flags,
                          std::unique_ptr<BufferTextureBinder> texture_binder)
    : gbm_handle{handle},
      bo_flags{bo_flags},
      texture_binder{std::move(texture_binder)},
      prime_fd{-1}
{
    auto device = gbm_bo_get_device(gbm_handle.get());
    auto gem_handle = gbm_bo_get_handle(gbm_handle.get()).u32;
    auto drm_fd = gbm_device_get_fd(device);

    auto ret = drmPrimeHandleToFD(drm_fd, gem_handle, DRM_CLOEXEC, &prime_fd);

    if (ret)
    {
        std::string const msg("Failed to get PRIME fd from gbm bo");
        BOOST_THROW_EXCEPTION(
            boost::enable_error_info(
                std::runtime_error(msg)) << boost::errinfo_errno(errno));
    }
}

mgx::GBMBuffer::~GBMBuffer()
{
    if (prime_fd > 0)
        close(prime_fd);
}

geom::Size mgx::GBMBuffer::size() const
{
    return {gbm_bo_get_width(gbm_handle.get()), gbm_bo_get_height(gbm_handle.get())};
}

geom::Stride mgx::GBMBuffer::stride() const
{
    return geom::Stride(gbm_bo_get_stride(gbm_handle.get()));
}

MirPixelFormat mgx::GBMBuffer::pixel_format() const
{
    return gbm_format_to_mir_format(gbm_bo_get_format(gbm_handle.get()));
}

void mgx::GBMBuffer::gl_bind_to_texture()
{
    texture_binder->gl_bind_to_texture();
}

std::shared_ptr<MirNativeBuffer> mgx::GBMBuffer::native_buffer_handle() const
{
    auto temp = std::make_shared<GBMNativeBuffer>();

    temp->fd_items = 1;
    temp->fd[0] = prime_fd;
    temp->stride = stride().as_uint32_t();
    temp->flags = (bo_flags & GBM_BO_USE_SCANOUT) ? mir_buffer_flag_can_scanout : 0;
    temp->bo = gbm_handle.get();

    auto const& dim = size();
    temp->width = dim.width.as_int();
    temp->height = dim.height.as_int();

    return temp;
}

void mgx::GBMBuffer::write(unsigned char const* /* pixels */, size_t /* size */)
{
    BOOST_THROW_EXCEPTION(std::runtime_error("Direct write to GBM hardware allocated buffer not supported"));
}

void mgx::GBMBuffer::read(std::function<void(unsigned char const*)> const& /* do_with_pixels */)
{
    BOOST_THROW_EXCEPTION(std::runtime_error("Direct read from GBM hardware allocated buffer not supported"));
}
