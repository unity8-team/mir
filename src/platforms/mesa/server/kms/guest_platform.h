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
 * Authored by:
 * Eleni Maria Stea <elenimaria.stea@canonical.com>
 * Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_GRAPHICS_MESA_GUEST_PLATFORM_H_
#define MIR_GRAPHICS_MESA_GUEST_PLATFORM_H_

#include "mir/graphics/platform.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/renderer/gl/egl_platform.h"
#include "display_helpers.h"

namespace mir
{
namespace graphics
{
namespace mesa
{
class PlatformAuthentication;

class GuestPlatform : public graphics::Platform,
                      public graphics::NativeRenderingPlatform,
                      public mir::renderer::gl::EGLPlatform
{
public:
    GuestPlatform(std::shared_ptr<graphics::PlatformAuthentication> const& platform_authentication_arg);

    UniqueModulePtr<GraphicBufferAllocator> create_buffer_allocator() override;
    UniqueModulePtr<PlatformIpcOperations> make_ipc_operations() const override;

    UniqueModulePtr<Display> create_display(
        std::shared_ptr<graphics::DisplayConfigurationPolicy> const&,
        std::shared_ptr<graphics::GLConfig> const& /*gl_config*/) override;
    NativeDisplayPlatform* native_display_platform() override;
    std::vector<ExtensionDescription> extensions() const override;

    NativeRenderingPlatform* native_rendering_platform() override;
    EGLNativeDisplayType egl_native_display() const override;

private:
    std::shared_ptr<graphics::PlatformAuthentication> const platform_authentication;
    std::shared_ptr<graphics::NativeDisplayPlatform> auth;
    helpers::GBMHelper gbm;
};
}
}
}

#endif // MIR_GRAPHICS_MESA_GUEST_PLATFORM_H_
