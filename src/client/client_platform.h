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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */
#ifndef MIR_CLIENT_CLIENT_PLATFORM_H_
#define MIR_CLIENT_CLIENT_PLATFORM_H_

#include <EGL/eglplatform.h>
#include <memory>

namespace mir
{
namespace client
{
class ClientBufferFactory;
class ClientSurface;
class ClientContext;

class ClientPlatform
{
public:
    ClientPlatform() = default;
    ClientPlatform(const ClientPlatform& p) = delete;
    ClientPlatform& operator=(const ClientPlatform& p) = delete;

    virtual std::shared_ptr<ClientBufferFactory> create_buffer_factory() = 0;
    virtual std::shared_ptr<EGLNativeWindowType> create_egl_native_window(ClientSurface *surface) = 0;
    virtual std::shared_ptr<EGLNativeDisplayType> create_egl_native_display() = 0;
};

}
}

#endif /* MIR_CLIENT_CLIENT_PLATFORM_H_ */
