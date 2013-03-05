/*
 * Copyright (C) 2010 LunarG Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Chia-I Wu <olv@lunarg.com>
 */

#include <mir_client_library.h>

#include <string.h>

#include "eglutint.h"

void connection_callback(MirConnection *conn, void *ctx)
{
  _eglut->native_dpy = conn;
}


void
_eglutNativeInitDisplay(void)
{
  mir_wait_for(mir_connect("/tmp/mir_socket", "eglut", connection_callback, NULL));
  
  if (!_eglut->native_dpy)
    _eglutFatal("failed to initialize native display");
  
  _eglut->surface_type = EGL_WINDOW_BIT;
}

void
_eglutNativeFiniDisplay(void)
{
  mir_connection_release(_eglut->native_dpy);
}

static void
surface_create_callback(MirSurface *surf, void *ctx)
{
  struct eglut_window *win = ctx;

  win->native.u.window = surf;
}

void
_eglutNativeInitWindow(struct eglut_window *win, const char *title,
                       int x, int y, int w, int h)
{
  MirSurfaceParameters parameters;
  parameters.name = strdup(title);
  parameters.height = h;
  parameters.width = w;
  parameters.pixel_format = mir_pixel_format_argb_8888;
  mir_wait_for(mir_surface_create(_eglut->native_dpy, &parameters,
				  surface_create_callback, win));
  win->native.width = w;
  win->native.height = h;
}

void
_eglutNativeFiniWindow(struct eglut_window *win)
{
  mir_surface_release(win->native.u.window, NULL, NULL);
}

void
_eglutNativeEventLoop(void)
{
   while (1) {
      struct eglut_window *win = _eglut->current;

      if (_eglut->idle_cb)
	_eglut->idle_cb();

      if (win->display_cb)
	win->display_cb();
      eglSwapBuffers(_eglut->dpy, win->surface);
   }
}
