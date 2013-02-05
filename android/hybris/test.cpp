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
 * Authored by: Thomas Voß <thomas.voss@canonical.com>
 */
#include <ubuntu/application/ui/init.h>
#include <ubuntu/application/ui/session.h>
#include <ubuntu/application/ui/session_credentials.h>
#include <ubuntu/application/ui/setup.h>

#include <ubuntu/ui/session_service.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <cassert>
#include <cstdio>

namespace
{
struct InputListener : public ubuntu::application::ui::input::Listener
{
    void on_new_event(const Event& /*ev*/)
    {
        printf("%s \n", __PRETTY_FUNCTION__);
    }
};

struct View
{
    static const char* vertex_shader();
    static const char* fragment_shader();

    static GLuint load_shader(GLenum shaderType, const char* pSource);
    static GLuint create_program(const char* pVertexSource, const char* pFragmentSource);

    static const GLfloat* triangle()
    {
        static const GLfloat result[] =
        {
            -0.125f, -0.125f, 0.0f, 0.5f,
            0.0f,  0.125f, 0.0f, 0.5f,
            0.125f, -0.125f, 0.0f, 0.5f
        };

        return result;
    }

    static const GLfloat* color_triangle()
    {
        static const GLfloat result[] =
        {
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 0.0f, 0.0f
        };

        return result;
    }

    View(const ubuntu::application::ui::Surface::Ptr& surface);

    void render();
    void step();

    ubuntu::application::ui::Surface::Ptr surface;
    EGLDisplay egl_display;
    EGLSurface egl_surface;
    EGLConfig egl_config;
    EGLContext egl_context;

    GLfloat rotation_angle;

    GLuint gProgram;
    GLuint gvPositionHandle, gvColorHandle;
    GLuint rotation_uniform;
    GLint num_vertex;
    const GLfloat * vertex_data;
    const GLfloat * color_data;
};

}

int main(int argc, char** argv)
{
    ubuntu::application::ui::init(argc, argv);
    ubuntu::application::ui::Setup::instance();

    ::SessionCredentials sc;
    memset(&sc, 0, sizeof(sc));
    snprintf(sc.application_name, sizeof(sc.application_name), "TestTestTest");
    
    ubuntu::application::ui::SessionCredentials creds(&sc);

    ubuntu::application::ui::Session::Ptr session =
        ubuntu::ui::SessionService::instance()->start_a_new_session(creds);

    ubuntu::application::ui::PhysicalDisplayInfo::Ptr p =
        session->physical_display_info(ubuntu::application::ui::primary_physical_display);

    printf("Resolution: (%dx%d)\n", p->horizontal_resolution(), p->vertical_resolution());
    ubuntu::application::ui::SurfaceProperties props =
    {
        "MainActorSurface",
        500,
        500,
        ubuntu::application::ui::main_actor_role
    };

    ubuntu::application::ui::Surface::Ptr surface =
        session->create_surface(
            props,
            ubuntu::application::ui::input::Listener::Ptr(new InputListener()));

    surface->set_alpha(0.5f);
    View view(surface);
    int x = 0;
    while (true)
    {
        view.render();
        view.step();
        x++;
        surface->move_to(x % p->horizontal_resolution(), 0);
    }
}

namespace
{

const char* View::vertex_shader()
{
    static const char shader[] =
        "attribute vec4 vPosition;\n"
        "attribute vec4 vColor;\n"
        "uniform float angle;\n"
        "varying vec4 colorinfo;\n"
        "void main() {\n"
        "  mat3 rot_z = mat3( vec3( cos(angle),  sin(angle), 0.0),\n"
        "                     vec3(-sin(angle),  cos(angle), 0.0),\n"
        "                     vec3(       0.0,         0.0, 1.0));\n"
        "  gl_Position = vec4(rot_z * vPosition.xyz, 1.0);\n"
        "  colorinfo = vColor;\n"
        "}\n";

    return shader;
}

const char* View::fragment_shader()
{
    static const char shader[] =
        "precision mediump float;\n"
        "varying vec4 colorinfo;\n"
        "void main() {\n"
        "  gl_FragColor = colorinfo;\n"
        "}\n";

    return shader;
}

GLuint View::load_shader(GLenum shaderType, const char* pSource)
{
    GLuint shader = glCreateShader(shaderType);
    if (shader)
    {
        glShaderSource(shader, 1, &pSource, NULL);
        glCompileShader(shader);
        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            GLint infoLen = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen)
            {
                char* buf = (char*) malloc(infoLen);
                if (buf)
                {
                    glGetShaderInfoLog(shader, infoLen, NULL, buf);
                    fprintf(stderr, "Could not compile shader %d:\n%s\n",
                            shaderType, buf);
                    free(buf);
                }
                glDeleteShader(shader);
                shader = 0;
            }
        }
    }
    else
    {
        printf("Error, during shader creation: %i\n", glGetError());
    }
    return shader;
}

GLuint View::create_program(const char* pVertexSource, const char* pFragmentSource)
{
    GLuint vertexShader = load_shader(GL_VERTEX_SHADER, pVertexSource);
    if (!vertexShader)
    {
        printf("vertex shader not compiled\n");
        return 0;
    }

    GLuint pixelShader = load_shader(GL_FRAGMENT_SHADER, pFragmentSource);
    if (!pixelShader)
    {
        printf("frag shader not compiled\n");
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program)
    {
        glAttachShader(program, vertexShader);
        glAttachShader(program, pixelShader);
        glLinkProgram(program);
        GLint linkStatus = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linkStatus);
        if (linkStatus != GL_TRUE)
        {
            GLint bufLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufLength);
            if (bufLength)
            {
                char* buf = (char*) malloc(bufLength);
                if (buf)
                {
                    glGetProgramInfoLog(program, bufLength, NULL, buf);
                    fprintf(stderr, "Could not link program:\n%s\n", buf);
                    free(buf);
                }
            }
            glDeleteProgram(program);
            program = 0;
        }
    }
    return program;
}

View::View(const ubuntu::application::ui::Surface::Ptr& surface)
    : surface(surface),
      rotation_angle(0.f),
      num_vertex(3)
{
    // assert(eglBindAPI(EGL_OPENGL_ES_API) == EGL_TRUE);
    egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    assert(egl_display != EGL_NO_DISPLAY);
    EGLint major, minor;
    if (EGL_FALSE == eglInitialize(egl_display, &major, &minor))
    {
        printf("egl error: problem initializing.\n");
        exit(1);
    }

    EGLint attribs[] =
    {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_NONE
    };
    EGLint n;
    if (EGL_FALSE == eglChooseConfig(
                egl_display,
                attribs,
                &egl_config,
                1,
                &n))
    {
        printf("egl error: Cannot choose configuration.\n");
    }

    EGLint context_attribs[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    egl_context = eglCreateContext(
                      egl_display,
                      egl_config,
                      EGL_NO_CONTEXT,
                      context_attribs);

    assert(EGL_NO_CONTEXT != eglContext);

    EGLNativeWindowType nativeWindow = surface->to_native_window_type();
    egl_surface = eglCreateWindowSurface(egl_display, egl_config, nativeWindow, NULL);

    eglMakeCurrent(
        egl_display,
        egl_surface,
        egl_surface,
        egl_context);

    vertex_data = triangle();
    color_data = color_triangle();

    gProgram = create_program(vertex_shader(), fragment_shader());
    if (!gProgram)
    {
        printf("error making program\n");
        return;
    }

    gvPositionHandle = glGetAttribLocation(gProgram, "vPosition");
    gvColorHandle = glGetAttribLocation(gProgram, "vColor");

    rotation_uniform = glGetUniformLocation(gProgram, "angle");

    return;
}

void View::render()
{
    glUseProgram(gProgram);

    glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

    glUniform1fv(rotation_uniform, 1, &rotation_angle);

    glVertexAttribPointer(
        gvColorHandle,
        num_vertex,
        GL_FLOAT,
        GL_FALSE,
        sizeof(GLfloat)*4, color_data);
    glVertexAttribPointer(
        gvPositionHandle,
        num_vertex,
        GL_FLOAT,
        GL_FALSE,
        0,
        vertex_data);
    glEnableVertexAttribArray(gvPositionHandle);
    glEnableVertexAttribArray(gvColorHandle);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, num_vertex);
    glDisableVertexAttribArray(gvPositionHandle);
    glDisableVertexAttribArray(gvColorHandle);

    eglSwapBuffers(egl_display, egl_surface);
}

void View::step()
{
    rotation_angle += 0.01;
}

}
