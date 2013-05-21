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
 * Author: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef __EGLAPP_H__
#define __EGLAPP_H__

#include <EGL/egl.h>

#ifdef __cplusplus
extern "C" {
#endif

void kvant_mir_shutdown(void);
void kvant_mir_connect(EGLNativeDisplayType *display, EGLNativeWindowType* window,
                       int width, int height);

#ifdef __cplusplus
}
#endif

#endif
