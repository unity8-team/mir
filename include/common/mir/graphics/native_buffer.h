/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_NATIVE_BUFFER_H_
#define MIR_GRAPHICS_NATIVE_BUFFER_H_

#ifndef MIR_BUILD_PLATFORM_ANDROID
#include <mir_toolkit/mir_native_buffer.h>
#endif

namespace mir
{
namespace graphics
{

#ifdef MIR_BUILD_PLATFORM_ANDROID
//just a fwd dcl
class NativeBuffer;
#else
typedef struct MirBufferPackage NativeBuffer;
#endif

}
}

#endif /* MIR_GRAPHICS_NATIVE_BUFFER_H_ */
