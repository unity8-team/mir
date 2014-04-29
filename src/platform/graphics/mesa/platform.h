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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_MESA_PLATFORM_H_
#define MIR_GRAPHICS_MESA_PLATFORM_H_

#include "mir/graphics/platform.h"
#include "mir/graphics/drm_authenticator.h"
#include "display_helpers.h"

#include "mir_toolkit/mesa/native_display.h"

namespace mir
{
namespace graphics
{
namespace mesa
{
enum class BypassOption
{
    allowed,
    prohibited
};

class VirtualTerminal;
class InternalNativeDisplay;
class Platform : public graphics::Platform,
                 public DRMAuthenticator,
                 public std::enable_shared_from_this<Platform>
{
public:
    explicit Platform(std::shared_ptr<DisplayReport> const& reporter,
                      std::shared_ptr<VirtualTerminal> const& vt,
                      BypassOption bypass_option);
    ~Platform();

    /* From Platform */
    std::shared_ptr<graphics::GraphicBufferAllocator> create_buffer_allocator(
            const std::shared_ptr<BufferInitializer>& buffer_initializer);
    std::shared_ptr<graphics::Display> create_display(
        std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy,
        std::shared_ptr<GLProgramFactory> const& program_factory,
        std::shared_ptr<GLConfig> const& gl_config);
    std::shared_ptr<PlatformIPCPackage> get_ipc_package();
    std::shared_ptr<InternalClient> create_internal_client();

    void fill_ipc_package(BufferIPCPacker* packer, Buffer const* buffer) const;

    EGLNativeDisplayType egl_native_display() const;

    /* From DRMAuthenticator */
    void drm_auth_magic(unsigned int magic);

    std::shared_ptr<mir::udev::Context> udev;
    helpers::DRMHelper drm;
    helpers::GBMHelper gbm;

    std::shared_ptr<DisplayReport> const listener;
    std::shared_ptr<VirtualTerminal> const vt;

    BypassOption bypass_option() const;

    //connection shared by all internal clients
    static bool internal_display_clients_present;
    static std::shared_ptr<InternalNativeDisplay> internal_native_display;
private:
    BypassOption const bypass_option_;
};

extern "C" int mir_server_mesa_egl_native_display_is_valid(MirMesaEGLNativeDisplay* display);

}
}
}
#endif /* MIR_GRAPHICS_MESA_PLATFORM_H_ */
