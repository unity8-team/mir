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
 * Thomas Voss <thomas.voss@canonical.com>
 * Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir_test_doubles/mock_egl.h"
#include <gtest/gtest.h>

namespace mtd = mir::test::doubles;

namespace
{
mtd::MockEGL* global_mock_egl = NULL;
}


EGLConfig configs[] =
{
    (void*)3,
    (void*)4,
    (void*)8,
    (void*)14
};
EGLint config_size = 4;

/* We prefix GL/EGL extensions with "extension_" so code under test has to get their function
   ptrs with eglGetProcAddress */
EGLImageKHR extension_eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target,
                                        EGLClientBuffer buffer, const EGLint *attrib_list);
EGLBoolean extension_eglDestroyImageKHR (EGLDisplay dpy, EGLImageKHR image);
void extension_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image);

/* EGL{Surface,Display,Config,Context} are all opaque types, so we can put whatever
   we want in them for testing */
mtd::MockEGL::MockEGL()
    : fake_egl_display((EGLDisplay) 0x0530),
      fake_configs(configs),
      fake_configs_num(config_size),
      fake_egl_surface((EGLSurface) 0xa034),
      fake_egl_context((EGLContext) 0xbeef),
      fake_egl_image((EGLImageKHR) 0x1234),
      fake_visual_id(1) //HAL_PIXEL_FORMAT_RGBA on android
{
    using namespace testing;
    assert(global_mock_egl == NULL && "Only one mock object per process is allowed");

    global_mock_egl = this;

    ON_CALL(*this, eglGetDisplay(_))
    .WillByDefault(Return(fake_egl_display));
    ON_CALL(*this, eglInitialize(_,_,_))
    .WillByDefault(DoAll(
                       SetArgPointee<1>(1),
                       SetArgPointee<2>(4),
                       Return(EGL_TRUE)));
    ON_CALL(*this, eglBindApi(EGL_OPENGL_ES_API))
    .WillByDefault(Return(EGL_TRUE));

    ON_CALL(*this, eglGetConfigs(_,NULL, 0, _))
    .WillByDefault(DoAll(
                       SetArgPointee<3>(config_size),
                       Return(EGL_TRUE)));

    ON_CALL(*this, eglGetConfigAttrib(_, _, EGL_NATIVE_VISUAL_ID, _))
    .WillByDefault(DoAll(
                       SetArgPointee<3>(fake_visual_id),
                       Return(EGL_TRUE)));

    ON_CALL(*this, eglChooseConfig(_,_,_,_,_))
    .WillByDefault(Invoke(
        [&] (EGLDisplay, const EGLint *, EGLConfig *configs,
             EGLint config_size, EGLint *num_config) -> EGLBoolean
        {
            EGLint const min_size = std::min(config_size, fake_configs_num);
            std::copy(fake_configs, fake_configs + min_size, configs);
            *num_config = min_size;
            return EGL_TRUE;
        }));

    ON_CALL(*this, eglCreateWindowSurface(_,_,_,_))
    .WillByDefault(Return(fake_egl_surface));

    ON_CALL(*this, eglCreatePbufferSurface(_,_,_))
    .WillByDefault(Return(fake_egl_surface));

    ON_CALL(*this, eglCreateContext(_,_,_,_))
    .WillByDefault(Return(fake_egl_context));

    ON_CALL(*this, eglMakeCurrent(_,_,_,_))
    .WillByDefault(Return(EGL_TRUE));

    ON_CALL(*this, eglSwapBuffers(_,_))
    .WillByDefault(Return(EGL_TRUE));

    ON_CALL(*this, eglGetCurrentDisplay())
    .WillByDefault(Return(fake_egl_display));

    ON_CALL(*this, eglCreateImageKHR(_,_,_,_,_))
    .WillByDefault(Return(fake_egl_image));

    typedef mtd::MockEGL::generic_function_pointer_t func_ptr_t;
    ON_CALL(*this, eglGetProcAddress(StrEq("eglCreateImageKHR")))
        .WillByDefault(Return(reinterpret_cast<func_ptr_t>(extension_eglCreateImageKHR)));
    ON_CALL(*this, eglGetProcAddress(StrEq("eglDestroyImageKHR")))
        .WillByDefault(Return(reinterpret_cast<func_ptr_t>(extension_eglDestroyImageKHR)));
    ON_CALL(*this, eglGetProcAddress(StrEq("glEGLImageTargetTexture2DOES")))
        .WillByDefault(Return(reinterpret_cast<func_ptr_t>(extension_glEGLImageTargetTexture2DOES)));
}

mtd::MockEGL::~MockEGL()
{
    global_mock_egl = NULL;
}

#define CHECK_GLOBAL_MOCK(rettype)         \
    if (!global_mock_egl)                  \
    {                                      \
        using namespace ::testing;         \
        ADD_FAILURE_AT(__FILE__,__LINE__); \
        rettype type = (rettype) 0;        \
        return type;                       \
    }

#define CHECK_GLOBAL_VOID_MOCK()            \
    if (!global_mock_egl)                   \
    {                                       \
        using namespace ::testing;          \
        ADD_FAILURE_AT(__FILE__,__LINE__);  \
        return;                             \
    }

EGLint eglGetError (void)
{
    CHECK_GLOBAL_MOCK(EGLint)
    return global_mock_egl->eglGetError();
}

EGLDisplay eglGetDisplay (NativeDisplayType display)
{
    CHECK_GLOBAL_MOCK(EGLDisplay);
    return global_mock_egl->eglGetDisplay(display);
}

EGLBoolean eglInitialize (EGLDisplay dpy, EGLint *major, EGLint *minor)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglInitialize(dpy, major, minor);
}

EGLBoolean eglTerminate (EGLDisplay dpy)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglTerminate(dpy);
}

const char * eglQueryString (EGLDisplay dpy, EGLint name)
{
    CHECK_GLOBAL_MOCK(const char *)
    return global_mock_egl->eglQueryString(dpy, name);
}

EGLBoolean eglBindAPI (EGLenum api)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglBindApi(api);
}

mtd::MockEGL::generic_function_pointer_t eglGetProcAddress (const char *name)
{
    CHECK_GLOBAL_MOCK(mtd::MockEGL::generic_function_pointer_t)
    return global_mock_egl->eglGetProcAddress(name);
}

EGLBoolean eglGetConfigs (EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglGetConfigs(dpy, configs, config_size, num_config);
}

EGLBoolean eglChooseConfig (EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);
}

EGLBoolean eglGetConfigAttrib (EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglGetConfigAttrib(dpy, config, attribute, value);
}

EGLSurface eglCreateWindowSurface (EGLDisplay dpy, EGLConfig config, NativeWindowType window, const EGLint *attrib_list)
{
    CHECK_GLOBAL_MOCK(EGLSurface)
    return global_mock_egl->eglCreateWindowSurface(dpy, config, window, attrib_list);
}

EGLSurface eglCreatePixmapSurface (EGLDisplay dpy, EGLConfig config, NativePixmapType pixmap, const EGLint *attrib_list)
{
    CHECK_GLOBAL_MOCK(EGLSurface)
    return global_mock_egl->eglCreatePixmapSurface(dpy, config, pixmap, attrib_list);
}

EGLSurface eglCreatePbufferSurface (EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
{
    CHECK_GLOBAL_MOCK(EGLSurface)
    return global_mock_egl->eglCreatePbufferSurface(dpy, config, attrib_list);
}

EGLBoolean eglDestroySurface (EGLDisplay dpy, EGLSurface surface)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglDestroySurface(dpy, surface);
}

EGLBoolean eglQuerySurface (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglQuerySurface(dpy, surface, attribute, value);
}

/* EGL 1.1 render-to-texture APIs */
EGLBoolean eglSurfaceAttrib (EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint value)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglSurfaceAttrib(dpy, surface, attribute, value);
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglBindTexImage(dpy, surface, buffer);
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglReleaseTexImage(dpy, surface, buffer);
}

/* EGL 1.1 swap control API */
EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglSwapInterval(dpy, interval);
}

EGLContext eglCreateContext (EGLDisplay dpy, EGLConfig config, EGLContext share_list, const EGLint *attrib_list)
{
    CHECK_GLOBAL_MOCK(EGLContext)
    return global_mock_egl->eglCreateContext(dpy, config, share_list, attrib_list);
}

EGLBoolean eglDestroyContext (EGLDisplay dpy, EGLContext ctx)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglDestroyContext(dpy, ctx);
}

EGLBoolean eglMakeCurrent (EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglMakeCurrent(dpy, draw, read, ctx);
}

EGLContext eglGetCurrentContext (void)
{
    CHECK_GLOBAL_MOCK(EGLContext)
    return global_mock_egl->eglGetCurrentContext();
}

EGLSurface eglGetCurrentSurface (EGLint readdraw)
{
    CHECK_GLOBAL_MOCK(EGLSurface)
    return global_mock_egl->eglGetCurrentSurface(readdraw);
}

EGLDisplay eglGetCurrentDisplay (void)
{
    CHECK_GLOBAL_MOCK(EGLDisplay)
    return global_mock_egl->eglGetCurrentDisplay();
}

EGLBoolean eglQueryContext (EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglQueryContext(dpy, ctx, attribute, value);
}

EGLBoolean eglWaitGL (void)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglWaitGL();
}

EGLBoolean eglWaitNative (EGLint engine)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglWaitNative(engine);
}

EGLBoolean eglSwapBuffers (EGLDisplay dpy, EGLSurface draw)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglSwapBuffers(dpy, draw);
}

EGLBoolean eglCopyBuffers (EGLDisplay dpy, EGLSurface surface, NativePixmapType target)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglCopyBuffers(dpy, surface, target);
}

/* extensions */
EGLImageKHR extension_eglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
{
    CHECK_GLOBAL_MOCK(EGLImageKHR)
    return global_mock_egl->eglCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
}

EGLBoolean extension_eglDestroyImageKHR (EGLDisplay dpy, EGLImageKHR image)
{
    CHECK_GLOBAL_MOCK(EGLBoolean)
    return global_mock_egl->eglDestroyImageKHR(dpy, image);
}

void extension_glEGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image)
{
    CHECK_GLOBAL_VOID_MOCK();
    global_mock_egl->glEGLImageTargetTexture2DOES(target, image);
}
