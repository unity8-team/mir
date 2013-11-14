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
 *   Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "gbm_buffer_allocator.h"
#include "gbm_buffer.h"
#include "gbm_platform.h"
#include "buffer_texture_binder.h"
#include "mir/graphics/buffer_initializer.h"
#include "mir/graphics/egl_extensions.h"
#include "mir/graphics/buffer_properties.h"

#include <posix/this_process.h>

#include <boost/throw_exception.hpp>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <algorithm>
#include <stdexcept>
#include <gbm.h>
#include <cassert>

namespace mg  = mir::graphics;
namespace mgg = mir::graphics::gbm;
namespace geom = mir::geometry;

namespace
{

class EGLImageBufferTextureBinder : public mgg::BufferTextureBinder
{
public:
    EGLImageBufferTextureBinder(std::shared_ptr<gbm_bo> const& gbm_bo,
                                std::shared_ptr<mg::EGLExtensions> const& egl_extensions)
        : bo{gbm_bo}, egl_extensions{egl_extensions}, egl_image{EGL_NO_IMAGE_KHR}
    {
    }

    ~EGLImageBufferTextureBinder()
    {
        if (egl_image != EGL_NO_IMAGE_KHR)
            egl_extensions->eglDestroyImageKHR(egl_display, egl_image);
    }


    void bind_to_texture()
    {
        ensure_egl_image();

        egl_extensions->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, egl_image);
    }

private:
    void ensure_egl_image()
    {
        if (egl_image == EGL_NO_IMAGE_KHR)
        {
            egl_display = eglGetCurrentDisplay();
            gbm_bo* bo_raw{bo.get()};

            const EGLint image_attrs[] =
            {
                EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
                EGL_NONE
            };

            egl_image = egl_extensions->eglCreateImageKHR(egl_display, EGL_NO_CONTEXT,
                                                          EGL_NATIVE_PIXMAP_KHR,
                                                          reinterpret_cast<void*>(bo_raw),
                                                          image_attrs);
            if (egl_image == EGL_NO_IMAGE_KHR)
                BOOST_THROW_EXCEPTION(std::runtime_error("Failed to create EGLImage from GBM bo"));
        }
    }

    std::shared_ptr<gbm_bo> const bo;
    std::shared_ptr<mg::EGLExtensions> const egl_extensions;
    EGLDisplay egl_display;
    EGLImageKHR egl_image;
};

struct GBMBODeleter
{
    void operator()(gbm_bo* handle) const
    {
        if (handle)
            gbm_bo_destroy(handle);
    }
};

}

mgg::GBMBufferAllocator::GBMBufferAllocator(
        gbm_device* device,
        const std::shared_ptr<BufferInitializer>& buffer_initializer)
        : device(device),
          buffer_initializer(buffer_initializer),
          egl_extensions(std::make_shared<mg::EGLExtensions>())
{
    assert(buffer_initializer.get() != 0);

    static const std::string disable_bypass{"0"};
    bypass_env = disable_bypass != posix::this_process::env::get("MIR_BYPASS");
}

std::shared_ptr<mg::Buffer> mgg::GBMBufferAllocator::alloc_buffer(BufferProperties const& buffer_properties)
{
    uint32_t bo_flags{GBM_BO_USE_RENDERING};

    uint32_t gbm_format = mgg::mir_format_to_gbm_format(buffer_properties.format);

    if (!is_pixel_format_supported(buffer_properties.format) ||
        gbm_format == mgg::invalid_gbm_format)
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Trying to create GBM buffer with unsupported pixel format"));
    }

    /* Create the GBM buffer object */
    if (buffer_properties.usage == BufferUsage::software)
        bo_flags |= GBM_BO_USE_WRITE;

    /*
     * Bypass is generally only beneficial to hardware buffers where the
     * blitting happens on the GPU. For software buffers it is slower to blit
     * individual pixels from CPU to GPU memory, so don't do it.
     * Also try to avoid allocating scanout buffers for small surfaces that
     * are unlikely to ever be fullscreen.
     *
     * TODO: Be more intelligent about when to apply GBM_BO_USE_SCANOUT. That
     *       may have to come after buffer reallocation support (surface
     *       resizing). We may also want to check for
     *       mir_surface_state_fullscreen later when it's fully wired up.
     */
    if (bypass_env &&
        buffer_properties.usage == BufferUsage::hardware &&
        buffer_properties.size.width.as_uint32_t() >= 800 &&
        buffer_properties.size.height.as_uint32_t() >= 600)
    {
        bo_flags |= GBM_BO_USE_SCANOUT;
    }

    gbm_bo *bo_raw = gbm_bo_create(
        device,
        buffer_properties.size.width.as_uint32_t(),
        buffer_properties.size.height.as_uint32_t(),
        gbm_format,
        bo_flags);

    if (!bo_raw)
        BOOST_THROW_EXCEPTION(std::runtime_error("Failed to create GBM buffer object"));

    std::shared_ptr<gbm_bo> bo{bo_raw, GBMBODeleter()};

    std::unique_ptr<EGLImageBufferTextureBinder> texture_binder{
        new EGLImageBufferTextureBinder{bo, egl_extensions}};

    /* Create the GBMBuffer */
    std::shared_ptr<mg::Buffer> buffer{new GBMBuffer{bo, bo_flags, std::move(texture_binder)}};

    (*buffer_initializer)(*buffer);

    return buffer;
}

std::vector<geom::PixelFormat> mgg::GBMBufferAllocator::supported_pixel_formats()
{
    static std::vector<geom::PixelFormat> const pixel_formats{
        geom::PixelFormat::argb_8888,
        geom::PixelFormat::xrgb_8888
    };

    return pixel_formats;
}

bool mgg::GBMBufferAllocator::is_pixel_format_supported(geom::PixelFormat format)
{
    auto formats = supported_pixel_formats();

    auto iter = std::find(formats.begin(), formats.end(), format);

    return iter != formats.end();
}
