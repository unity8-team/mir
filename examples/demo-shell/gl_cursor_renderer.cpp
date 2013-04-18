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

#include <GLES2/gl2.h>

#include <assert.h>
#include <stdio.h>

namespace me = mir::examples;

namespace
{

char const vshadersrc[] =
    "attribute vec4 vPosition;            \n"
    "uniform float theta;                 \n"
    "void main()                          \n"
    "{                                    \n"
    "    float c = cos(theta);            \n"
    "    float s = sin(theta);            \n"
    "    mat2 m;                          \n"
    "    m[0] = vec2(c, s);               \n"
    "    m[1] = vec2(-s, c);              \n"
    "    vec2 p = m * vec2(vPosition);    \n"
    "    gl_Position = vec4(p, 0.0, 1.0); \n"
    "}                                    \n";

char const fshadersrc[] =
    "precision mediump float;             \n"
    "uniform vec4 col;                    \n"
    "void main()                          \n"
    "{                                    \n"
    "    gl_FragColor = col;              \n"
    "}                                    \n";

GLfloat const vertices[] =
    {
        0.0f, 1.0f,
       -1.0f,-0.866f,
        1.0f,-0.866f,
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

void me::GLCursorRenderer::render_cursor(int x, int y)
{
    // TODO: Scale and transform
    (void) x; (void) y;

    glUseProgram(resources.prog);

    auto vpos = glGetAttribLocation(resources.prog, "vPosition");
    auto col = glGetUniformLocation(resources.prog, "col");
    auto theta = glGetUniformLocation(resources.prog, "theta");
    glUniform4f(col, UBUNTU_ORANGE, 1.0f);
    glUniform1f(theta, 0.0f);

    glVertexAttribPointer(vpos, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);
    
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
