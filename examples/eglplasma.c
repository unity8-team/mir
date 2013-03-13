/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "eglapp.h"
#include <assert.h>
#include <stdio.h>
#include <GLES2/gl2.h>

static GLuint load_shader(const char *src, GLenum type)
{
    GLuint shader = glCreateShader(type);
    if (shader)
    {
        GLint compiled;
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLchar log[1024];
            glGetShaderInfoLog(shader, sizeof log - 1, NULL, log);
            log[sizeof log - 1] = '\0';
            printf("load_shader compile failed: %s\n", log);
            glDeleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

/* Colours from http://design.ubuntu.com/brand/colour-palette */
#define DARK_AUBERGINE 0.17254902f,  0.0f,         0.117647059f
#define MID_AUBERGINE  0.368627451f, 0.152941176f, 0.31372549f
#define ORANGE         0.866666667f, 0.282352941f, 0.141414141f

int main(void)
{
    const char vshadersrc[] =
        "attribute vec4 vPosition;                               \n"
        "varying vec2 texcoord;                                  \n"
        "void main()                                             \n"
        "{                                                       \n"
        "    gl_Position = vPosition;                            \n"
        "    texcoord = vec2(vPosition) * vec2(0.5) + vec2(0.5); \n"
        "}                                                       \n";

    const char fshadersrc[] =
        "precision mediump float;                                \n"
        "uniform float theta;                                    \n"
        "varying vec2 texcoord;                                  \n"
        "uniform vec4 low_color, high_color;                     \n"
        "void main()                                             \n"
        "{                                                       \n"
        "    const float pi2 = 6.283185308;                      \n"
        "    float u = texcoord.x * pi2;                         \n"
        "    float v = texcoord.y * pi2;                         \n"
        "    float t = mod(theta, pi2);                          \n"
        "    float us = (cos(1.1 * u + 7.0 * t) +                \n"
        "                cos(2.3 * v * cos(1.0 * t)) +           \n"
        "                cos(0.3 * u * cos(3.0 * t))             \n"
        "               ) / 3.0;                                 \n"
        "    float vs = (cos(2.3 * v + 8.0 * t) +                \n"
        "                cos(1.3 * u * cos(3.0 * t)) +           \n"
        "                cos(1.7 * v * cos(2.0 * t))             \n"
        "               ) / 3.0;                                 \n"
        "    float x = (us * vs + 1.0) / 2.0;                    \n"
        "    gl_FragColor = x * (high_color - low_color) +       \n"
        "                   low_color;                           \n"
        "}                                                       \n";

    const GLfloat vertices[] =
    {
       -1.0f, 1.0f,
        1.0f, 1.0f,
        1.0f,-1.0f,
       -1.0f,-1.0f,
    };
    GLuint vshader, fshader, prog;
    GLint linked, low_color, high_color, vpos, theta;
    int width = 0, height = 0;
    GLfloat angle = 0.0f;

    if (!mir_eglapp_init(&width, &height))
    {
        printf("Can't initialize EGL\n");
        return 1;
    }

    vshader = load_shader(vshadersrc, GL_VERTEX_SHADER);
    assert(vshader);
    fshader = load_shader(fshadersrc, GL_FRAGMENT_SHADER);
    assert(fshader);
    prog = glCreateProgram();
    assert(prog);
    glAttachShader(prog, vshader);
    glAttachShader(prog, fshader);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLchar log[1024];
        glGetProgramInfoLog(prog, sizeof log - 1, NULL, log);
        log[sizeof log - 1] = '\0';
        printf("Link failed: %s\n", log);
        return 2;
    }

    glViewport(0, 0, width, height);

    glUseProgram(prog);

    vpos = glGetAttribLocation(prog, "vPosition");
    low_color = glGetUniformLocation(prog, "low_color");
    high_color = glGetUniformLocation(prog, "high_color");
    theta = glGetUniformLocation(prog, "theta");
    glUniform4f(low_color, DARK_AUBERGINE, 1.0f);
    glUniform4f(high_color, ORANGE, 1.0f);

    glVertexAttribPointer(vpos, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(0);

    while (mir_eglapp_running())
    {
        glUniform1f(theta, angle);
        angle += 0.005f;
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        mir_eglapp_swap_buffers();
    }

    mir_eglapp_shutdown();

    return 0;
}
