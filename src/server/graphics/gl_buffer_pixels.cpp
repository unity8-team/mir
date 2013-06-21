/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored By: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/graphics/gl_context.h"
#include "mir/graphics/gl_buffer_pixels.h"
#include "mir/compositor/buffer.h"

#include <GLES2/gl2ext.h>

namespace mg = mir::graphics;
namespace msh = mir::shell;
namespace geom = mir::geometry;

mg::GLBufferPixels::GLBufferPixels(std::unique_ptr<GLContext> gl_context)
    : gl_context{std::move(gl_context)},
      tex{0}, fbo{0}, pixels_need_y_flip{false},
      pixel_data{geom::Size(), geom::Stride(), nullptr}
{
}

mg::GLBufferPixels::~GLBufferPixels() noexcept
{
    if (tex != 0)
        glDeleteTextures(1, &tex);
    if (fbo != 0)
        glDeleteFramebuffers(1, &fbo);
}

void mg::GLBufferPixels::prepare()
{
    gl_context->make_current();

    if (tex == 0)
        glGenTextures(1, &tex);

    glBindTexture(GL_TEXTURE_2D, tex);
    glActiveTexture(GL_TEXTURE0);

    if (fbo == 0)
        glGenFramebuffers(1, &fbo);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void mg::GLBufferPixels::extract_from(compositor::Buffer& buffer)
{
    auto width = buffer.size().width.as_uint32_t();
    auto height = buffer.size().height.as_uint32_t();

    pixels.resize(width * height * 4);

    prepare();

    buffer.bind_to_texture();

    /*
     * TODO: Handle systems that don't support GL_BGRA_EXT for glReadPixels(),
     * or are big-endian (and therefore GL_BGRA doesn't give 0xAARRGGBB pixels).
     */
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glReadPixels(0, 0, width, height, GL_BGRA_EXT, GL_UNSIGNED_BYTE, pixels.data());

    pixel_data = {buffer.size(), geom::Stride{width * 4}, pixels.data()};
    pixels_need_y_flip = true;
}

msh::BufferPixelsData mg::GLBufferPixels::as_argb_8888()
{
    if (pixels_need_y_flip)
    {
        auto const stride = pixel_data.stride.as_uint32_t();
        auto const height = pixel_data.size.height.as_uint32_t();

        std::vector<char> tmp(stride);

        for (unsigned int i = 0; i < height / 2; i++)
        {
            /* Store line i */
            tmp.assign(&pixels[i * stride], &pixels[(i + 1) * stride]);

            /* Copy line height - i - 1 to line i */
            std::copy(&pixels[(height - i - 1) * stride], &pixels[(height - i) * stride],
                      &pixels[i * stride]);

            /* Copy stored line (i) to height - i - 1 */
            std::copy(tmp.begin(), tmp.end(), &pixels[(height - i - 1) * stride]);
        }

        pixels_need_y_flip = false;
    }

    return pixel_data;
}
