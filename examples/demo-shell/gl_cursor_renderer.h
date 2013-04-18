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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_EXAMPLES_GL_CURSOR_RENDERER_H_
#define MIR_EXAMPLES_GL_CURSOR_RENDERER_H_

#include <GLES2/gl2.h>

namespace mir
{
namespace examples
{
class GLCursorRenderer
{
public:
    GLCursorRenderer();
    ~GLCursorRenderer() = default;
    
    void render_cursor(int x, int y);

protected:
    GLCursorRenderer(GLCursorRenderer const&) = delete;
    GLCursorRenderer& operator=(GLCursorRenderer const&) = delete;

private:
    static int const cursor_width_px = 12;
    static int const cursor_height_px = 12;
    
    struct Resources
    {
    public: 
        Resources();
        ~Resources();

        GLuint fragment_shader;
        GLuint vertex_shader;
        GLuint prog;
    };

    Resources resources;
};

}
} // namespace mir

#endif // MIR_EXAMPLES_GL_CURSOR_RENDERER_H_
