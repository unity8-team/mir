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
 *   Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/egl_extensions.h"
#include "mir/graphics/android/sync_fence.h"
#include "android_format_conversion-inl.h"
#include "buffer.h"

#include <system/window.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <boost/throw_exception.hpp>
#include <stdexcept>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

mga::Buffer::Buffer(std::shared_ptr<NativeBuffer> const& buffer_handle,
                    std::shared_ptr<Fence> const& fence,
                    std::shared_ptr<mg::EGLExtensions> const& extensions)
    : native_buffer(buffer_handle),
      buffer_fence(fence),
      sync_ops(std::make_shared<mga::RealSyncFileOps>()),
      egl_extensions(extensions)
{
}

mga::Buffer::~Buffer()
{
    std::map<EGLDisplay,EGLImageKHR>::iterator it;
    for(it = egl_image_map.begin(); it != egl_image_map.end(); it++)
    {
        egl_extensions->eglDestroyImageKHR(it->first, it->second);
    }
}


geom::Size mga::Buffer::size() const
{
    return {native_buffer->width, native_buffer->height};
}

geom::Stride mga::Buffer::stride() const
{
    return geom::Stride{native_buffer->stride *
                        geom::bytes_per_pixel(pixel_format())};
}

geom::PixelFormat mga::Buffer::pixel_format() const
{
    return mga::to_mir_format(native_buffer->format);
}

bool mga::Buffer::can_bypass() const
{
    return false;
}

void mga::Buffer::bind_to_texture()
{
    //we are about to use the color buffer. make sure we own it, and its ready
    std::unique_lock<std::mutex> lk(content_lock);

    buffer_fence->wait();

    EGLDisplay disp = eglGetCurrentDisplay();
    if (disp == EGL_NO_DISPLAY) {
        BOOST_THROW_EXCEPTION(std::runtime_error("cannot bind buffer to texture without EGL context\n"));
    }
    static const EGLint image_attrs[] =
    {
        EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
        EGL_NONE
    };
    EGLImageKHR image;
    auto it = egl_image_map.find(disp);
    if (it == egl_image_map.end())
    {
        image = egl_extensions->eglCreateImageKHR(disp, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
                                  native_buffer.get(), image_attrs);
        if (image == EGL_NO_IMAGE_KHR)
        {
            BOOST_THROW_EXCEPTION(std::runtime_error("error binding buffer to texture\n"));
        }
        egl_image_map[disp] = image;
    }
    else /* already had it in map */
    {
        image = it->second;
    }

    egl_extensions->glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    //note: we should transfer the content_lock to the compositor until the rendering is complete
    //      since we can't do that yet, we rely on the swapper algorithm to ensure the texture is 
    //      owned by the compositor until the render is done
}

std::shared_ptr<mga::NativeBuffer> mga::Buffer::native_buffer_handle() const
{
    std::unique_lock<std::mutex> lk(content_lock);

    //copy the fence out to the native user
    native_buffer->fence = buffer_fence->copy_native_handle();
    return std::shared_ptr<mga::NativeBuffer>(
        native_buffer.get(),
        [this,&lk](NativeBuffer* buffer)
        {
            mga::SyncFence sync_fence(sync_ops, buffer->fence);
            buffer_fence->merge_with(sync_fence);
            lk.unlock();
        });
}
