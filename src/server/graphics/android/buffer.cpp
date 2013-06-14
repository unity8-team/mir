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

#include "buffer.h"
#include "graphic_alloc_adaptor.h"
#include "android_format_conversion-inl.h"

#include <system/window.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <dlfcn.h>

namespace mc=mir::compositor;
namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace geom=mir::geometry;

mga::Buffer::Buffer(const std::shared_ptr<GraphicAllocAdaptor>& alloc_device,
                    geom::Size size, geom::PixelFormat pf, mga::BufferUsage use)
{
    if (!alloc_device)
        BOOST_THROW_EXCEPTION(std::runtime_error("No allocation device for graphics buffer"));

    native_buffer = alloc_device->alloc_buffer(size, pf, use);

    //TODO: we could make the native buffer itself a constructor dependency and move creation
    //      failures to the factory
    if (!native_buffer.get())
        BOOST_THROW_EXCEPTION(std::runtime_error("Graphics buffer allocation failed"));

    this->eglCreateImageKHR = (EglCreateImageKHR)dlsym(RTLD_DEFAULT,
                                                       "eglCreateImageKHR");
}

mga::Buffer::~Buffer()
{
    std::map<EGLDisplay,EGLImageKHR>::iterator it;
    for(it = egl_image_map.begin(); it != egl_image_map.end(); it++)
    {
        eglDestroyImageKHR(it->first, it->second);
    }
}


geom::Size mga::Buffer::size() const
{
    return geom::Size{geom::Width{native_buffer->width},
                      geom::Height{native_buffer->height}};
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

void mga::Buffer::bind_to_texture()
{
    EGLDisplay disp = eglGetCurrentDisplay();
    if (disp == EGL_NO_DISPLAY) {
        BOOST_THROW_EXCEPTION(std::runtime_error("cannot bind buffer to texture without EGL context\n"));
    }
    static const EGLint image_attrs[] =
    {
        EGL_IMAGE_PRESERVED_KHR,    EGL_TRUE,
        EGL_NONE
    };
    EGLImageKHR image;
    auto it = egl_image_map.find(disp);
    if (it == egl_image_map.end())
    {
        image = this->eglCreateImageKHR(disp, EGL_NO_CONTEXT,
                                        EGL_NATIVE_BUFFER_ANDROID,
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

    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);

    return;
}
 
std::shared_ptr<ANativeWindowBuffer> mga::Buffer::native_buffer_handle() const
{
    return native_buffer; 
}
