/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_GL_TEXTURE_H_
#define MIR_GRAPHICS_GL_TEXTURE_H_

#include <GLES2/gl2.h>

namespace mir
{
namespace graphics
{
class GLTexture
{
public:
    GLTexture();
    ~GLTexture();
    void bind() const;

private:
    GLTexture(GLTexture const&) = delete;
    GLTexture& operator=(GLTexture const&) = delete;
    GLuint generate_id() const;
    GLuint const id;
};
}
}

#endif /* MIR_GRAPHICS_GL_TEXTURE_H_ */
