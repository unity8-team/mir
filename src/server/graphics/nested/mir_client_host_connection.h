/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_NESTED_MIR_CLIENT_HOST_CONNECTION_H_
#define MIR_GRAPHICS_NESTED_MIR_CLIENT_HOST_CONNECTION_H_

#include "host_connection.h"

#include <string>

struct MirConnection;

namespace ms=mir::scene;

namespace mir
{
namespace graphics
{
namespace nested
{

class MirClientHostConnection : public HostConnection
{
public:
    MirClientHostConnection(std::string const& host_socket, std::string const& name, std::shared_ptr<ms::SessionContainer> session_container);
    ~MirClientHostConnection();

    std::vector<int> platform_fd_items() override;
    EGLNativeDisplayType egl_native_display() override;
    std::shared_ptr<MirDisplayConfiguration> create_display_config() override;
    std::shared_ptr<HostSurface> create_surface(MirSurfaceParameters const&) override;
    void set_display_config_change_callback(std::function<void()> const& cb) override;
    void apply_display_config(MirDisplayConfiguration&) override;

    void drm_auth_magic(int magic) override;
    void drm_set_gbm_device(struct gbm_device* dev) override;

private:
    MirConnection* const mir_connection;
    std::function<void()> conf_change_callback;
    std::shared_ptr<ms::SessionContainer> session_container;
};

}
}
}

#endif /* MIR_GRAPHICS_NESTED_MIR_CLIENT_HOST_CONNECTION_H_ */
