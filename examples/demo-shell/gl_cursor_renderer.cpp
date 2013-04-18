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

#include "gl_cursor_renderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <GLES2/gl2.h>

#include <assert.h>
#include <stdio.h>

namespace me = mir::examples;
namespace geom = mir::geometry;

namespace
{

char const vshadersrc[] =
    "attribute vec4 vPosition;            \n"
    "uniform mat4 transform;              \n"
    "varying vec2 texcoord;               \n"
    "void main()                          \n"
    "{                                    \n"
    "    gl_Position = transform * vPosition; \n"
    "    texcoord = vec2(vPosition);      \n"
    "}                                    \n";

char const fshadersrc[] =
    "precision mediump float;             \n"
    "uniform vec4 col;                    \n"
    "varying vec2 texcoord;               \n"
    "void main()                          \n"
    "{                                    \n"
        "float border = 0.08;\n"
        "float radius = 0.5;\n"
        "vec2 m = texcoord - vec2(0.5, 0.5);\n"
        "float dist = radius - sqrt(m.x * m.x + m.y * m.y);\n"
        "float t = 0.0;\n"
        "if (dist > border)\n"
        "   gl_FragColor = vec4(1.0, 1.0, 1.0, 0.8);\n"
        "else if (dist > 0.0)\n"
        "   gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
        "else\n"
            "gl_FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
    "}                                    \n";

const GLint num_vertex = 4;
GLfloat vertex_data[] =
{
    -0.5f, -0.5f, 0.0f, 1.0f,
    -0.5f,  0.5f, 0.0f, 1.0f,
     0.5f, -0.5f, 0.0f, 1.0f,
     0.5f,  0.5f, 0.0f, 1.0f,
};

static GLuint load_shader(const char *src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    if (shader)
    {
        GLint compiled;
        glShaderSource(shader, 1, &src, 0);
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLchar log[1024];
            glGetShaderInfoLog(shader, sizeof log - 1, 0, log);
            log[sizeof log - 1] = '\0';
            printf("load_shader compile failed: %s\n", log);
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

static inline glm::mat4 compute_transformation(uint32_t display_width, uint32_t display_height, 
                                               float cursor_x, float cursor_y,
                                               uint32_t cursor_width, uint32_t cursor_height)
{
    glm::mat4 screen_to_gl_coords = glm::translate(glm::mat4{1.0f}, glm::vec3{-1.0f, 1.0f, 0.0f});
    screen_to_gl_coords = glm::scale(screen_to_gl_coords,
                                     glm::vec3{2.0f / display_width,
                                             -2.0f / display_height,
                                             1.0f});
    
    glm::vec3 cursor_size{cursor_width, cursor_height, 0.0f};
    glm::vec3 cursor_pos{cursor_x, cursor_y, 0.0f};
    glm::vec3 centered_cursor_pos{cursor_pos + 0.5f * cursor_size};
    
    glm::mat4 pos_size_matrix;
    pos_size_matrix = glm::translate(pos_size_matrix, centered_cursor_pos);
    pos_size_matrix = glm::scale(pos_size_matrix, cursor_size);
    
    return screen_to_gl_coords * pos_size_matrix;
}

}

me::GLCursorRenderer::Resources::Resources()
{
    vertex_shader = load_shader(vshadersrc, GL_VERTEX_SHADER);
    assert(vertex_shader);
    fragment_shader = load_shader(fshadersrc, GL_FRAGMENT_SHADER);
    assert(fragment_shader);
    
    prog = glCreateProgram();
    assert(prog);
    
    glAttachShader(prog, vertex_shader);
    glAttachShader(prog, fragment_shader);
    glLinkProgram(prog);

    int linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLchar log[1024];
        glGetProgramInfoLog(prog, sizeof log - 1, NULL, log);
        log[sizeof log - 1] = '\0';
        printf("Link failed: %s\n", log);
    }

    auto vpos = glGetAttribLocation(prog, "vPosition");
    glVertexAttribPointer(vpos, 4, GL_FLOAT, GL_FALSE, 0, vertex_data);
}

me::GLCursorRenderer::Resources::~Resources()
{
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(prog);
}

me::GLCursorRenderer::GLCursorRenderer()
{
}

#define UBUNTU_ORANGE 0.866666667f, 0.282352941f, 0.141414141f

void me::GLCursorRenderer::render_cursor(geom::Size const& display_size, float x, float y)
{
    // TODO: Scale and transform
    glUseProgram(resources.prog);

    auto col = glGetUniformLocation(resources.prog, "col");
    auto theta = glGetUniformLocation(resources.prog, "theta");
    auto vpos = glGetAttribLocation(resources.prog, "vPosition");
    glUniform4f(col, UBUNTU_ORANGE, 1.0f);
    glUniform1f(theta, 0.0f);
    
    auto transformation = compute_transformation(display_size.width.as_uint32_t(), display_size.height.as_uint32_t(),
                                                 x, y, cursor_width_px, cursor_height_px);
    auto transform_loc = glGetUniformLocation(resources.prog, "transform");
    glUniformMatrix4fv(transform_loc, 1, GL_FALSE, glm::value_ptr(transformation));

    glEnableVertexAttribArray(vpos);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(vpos);
}
