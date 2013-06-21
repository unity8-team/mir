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

#ifndef MIR_GRAPHICS_GL_BUFFER_PIXELS_H_
#define MIR_GRAPHICS_GL_BUFFER_PIXELS_H_

#include "mir/shell/buffer_pixels.h"

#include <memory>
#include <vector>

#include <GLES2/gl2.h>

namespace mir
{
namespace graphics
{

class GLContext;

/** Extracts the pixels from a buffer using GL facilities. */
class GLBufferPixels : public shell::BufferPixels
{
public:
    GLBufferPixels(std::unique_ptr<GLContext> gl_context);
    ~GLBufferPixels() noexcept;

    void extract_from(compositor::Buffer& buffer);
    shell::BufferPixelsData as_argb_8888();

private:
    void prepare();

    std::unique_ptr<GLContext> const gl_context;
    GLuint tex;
    GLuint fbo;
    std::vector<char> pixels;
    bool pixels_need_y_flip;
    shell::BufferPixelsData pixel_data;
};

}
}

#endif /* MIR_GRAPHICS_GL_BUFFER_PIXELS_H_ */
