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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_CLIENT_ANDROID_NATIVE_DISPLAY_CONTAINER_H_
#define MIR_CLIENT_ANDROID_NATIVE_DISPLAY_CONTAINER_H_

#include "../egl_native_display_container.h"

#include "mir_toolkit/c_types.h"

namespace mir
{
namespace client
{
namespace android
{

class AndroidNativeDisplayContainer : public EGLNativeDisplayContainer
{
public:
    AndroidNativeDisplayContainer();
    virtual ~AndroidNativeDisplayContainer();

    MirEGLNativeDisplayType create(MirConnection* connection);
    void release(MirEGLNativeDisplayType display);

    bool validate(MirEGLNativeDisplayType display) const;

protected:
    AndroidNativeDisplayContainer(AndroidNativeDisplayContainer const&) = delete;
    AndroidNativeDisplayContainer& operator=(AndroidNativeDisplayContainer const&) = delete;
};

}
}
} // namespace mir

#endif // MIR_CLIENT_ANDROID_NATIVE_DISPLAY_CONTAINER_H_
