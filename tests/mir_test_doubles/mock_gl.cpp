/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by:
 * Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir_test_doubles/mock_gl.h"
#include <gtest/gtest.h>

namespace mtd = mir::test::doubles;

namespace
{
mtd::MockGL* global_mock_gl = NULL;
}

mtd::MockGL::MockGL()
{
    using namespace testing;
    assert(global_mock_gl == NULL && "Only one mock object per process is allowed");

    global_mock_gl = this;

}

mtd::MockGL::~MockGL()
{
    global_mock_gl = NULL;
}

#define CHECK_GLOBAL_VOID_MOCK()            \
    if (!global_mock_gl)                    \
    {                                       \
        using namespace ::testing;          \
        ADD_FAILURE_AT(__FILE__,__LINE__);  \
        return;                             \
    }

#define CHECK_GLOBAL_MOCK(rettype)          \
    if (!global_mock_gl)                    \
    {                                       \
        using namespace ::testing;          \
        ADD_FAILURE_AT(__FILE__,__LINE__);  \
        rettype type = (rettype) 0;         \
        return type;                        \
    }

GLenum glGetError()
{
    CHECK_GLOBAL_MOCK(GLenum);
    return global_mock_gl->glGetError();
}

const GLubyte* glGetString(GLenum name)
{
    CHECK_GLOBAL_MOCK(const GLubyte*);
    return global_mock_gl->glGetString(name);
}

void glUseProgram(GLuint program)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glUseProgram (program);
}

void glEnable(GLenum func)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glEnable (func);
}

void glDisable(GLenum func)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glDisable(func);
}

void glBlendFunc(GLenum src, GLenum dst)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glBlendFunc (src, dst);
}

void glActiveTexture(GLenum unit)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glActiveTexture (unit);
}

void glUniformMatrix4fv(GLint location,
                        GLsizei count,
                        GLboolean transpose,
                        const GLfloat* value)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glUniformMatrix4fv(location, count, transpose, value);
}

void glUniform1f(GLint location, GLfloat x)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glUniform1f(location, x);
}

void glBindBuffer(GLenum buffer, GLuint name)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glBindBuffer(buffer, name);
}

void glVertexAttribPointer(GLuint indx,
                           GLint size,
                           GLenum type,
                           GLboolean normalized,
                           GLsizei stride,
                           const GLvoid* ptr)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
}

void glBindTexture(GLenum target, GLuint texture)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glBindTexture(target, texture);
}

void glEnableVertexAttribArray(GLuint index)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glEnableVertexAttribArray(index);
}

void glDisableVertexAttribArray(GLuint index)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glDisableVertexAttribArray(index);
}

void glDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glDrawArrays(mode, first, count);
}

GLuint glCreateShader(GLenum type)
{
    CHECK_GLOBAL_MOCK(GLuint);
    return global_mock_gl->glCreateShader(type);
}

/* This is the version of glShaderSource in Mesa < 9.0.1 */
void glShaderSource(GLuint shader, GLsizei count, const GLchar **string, const GLint *length)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glShaderSource (shader, count, string, length);
}

/* This is the version of glShaderSource in Mesa >= 9.0.1 */
void glShaderSource(GLuint shader, GLsizei count, const GLchar* const* string, const GLint *length)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glShaderSource (shader, count, string, length);
}

void glCompileShader(GLuint shader)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glCompileShader(shader);
}

void glGetShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGetShaderiv(shader, pname, params);
}

void glGetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGetShaderInfoLog(shader, bufsize, length, infolog);
}

GLuint glCreateProgram()
{
    CHECK_GLOBAL_MOCK(GLuint);
    return global_mock_gl->glCreateProgram();
}

void glAttachShader(GLuint program, GLuint shader)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glAttachShader(program, shader);
}

void glLinkProgram(GLuint program)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glLinkProgram(program);
}

GLint glGetUniformLocation(GLuint program, const GLchar *name)
{
    CHECK_GLOBAL_MOCK(GLint);
    return global_mock_gl->glGetUniformLocation(program, name);
}

GLint glGetAttribLocation(GLuint program, const GLchar *name)
{
    CHECK_GLOBAL_MOCK(GLint);
    return global_mock_gl->glGetAttribLocation(program, name);
}

void glTexParameteri(GLenum target, GLenum pname, GLint param)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glTexParameteri(target, pname, param);
}

void glGenTextures(GLsizei n, GLuint *textures)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGenTextures(n, textures);
}

void glUniform1i(GLint location, GLint x)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glUniform1i(location, x);
}

void glGenBuffers(GLsizei n, GLuint *buffers)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGenBuffers(n, buffers);
}

void glBufferData(GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glBufferData(target, size, data, usage);
}

void glGetProgramiv(GLuint program, GLenum pname, GLint *params)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGetProgramiv(program, pname, params);
}

void glGetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGetProgramInfoLog(program, bufsize, length, infolog);
}

void glTexImage2D(GLenum target, GLint level, GLint internalformat,
                  GLsizei width, GLsizei height, GLint border,
                  GLenum format, GLenum type, const GLvoid* pixels)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void glGenFramebuffers(GLsizei n, GLuint *framebuffers)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glGenFramebuffers(n, framebuffers);
}

void glBindFramebuffer(GLenum target, GLuint framebuffer)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glBindFramebuffer(target, framebuffer);
}

void glFramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget,
                            GLuint texture, GLint level)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glFramebufferTexture2D(target, attachment, textarget,
                                           texture, level);

}

void glReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, GLvoid* pixels)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_gl->glReadPixels(x, y, width, height, format, type, pixels);
}
