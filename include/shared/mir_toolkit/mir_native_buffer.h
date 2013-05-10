/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_CLIENT_MIR_NATIVE_BUFFER_H_
#define MIR_CLIENT_MIR_NATIVE_BUFFER_H_

enum { mir_buffer_package_max = 32 };
typedef struct MirBufferPackage
{
    int data_items;
    int fd_items;

    int data[mir_buffer_package_max];
    int fd[mir_buffer_package_max];

    int stride;
    int age; /**< Number of frames submitted by the client since the client has rendered to this buffer. */
             /**< This has the same semantics as the EGL_EXT_buffer_age extension */
             /**< \see http://www.khronos.org/registry/egl/extensions/EXT/EGL_EXT_buffer_age.txt */
} MirBufferPackage;

#ifdef ANDROID
struct ANativeWindowBuffer;
typedef struct ANativeWindowBuffer MirNativeBuffer;
#else
typedef struct MirBufferPackage MirNativeBuffer; 
#endif
#endif /* MIR_CLIENT_MIR_NATIVE_BUFFER_H_ */
