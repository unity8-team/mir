/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#ifndef MIR_GRAPHICS_X_PLATFORM_H_
#define MIR_GRAPHICS_X_PLATFORM_H_

#include "mir/graphics/platform.h"
#include "display_helpers.h"

namespace mir
{
namespace graphics
{
namespace X
{

class Platform : public graphics::Platform
{
public:
    explicit Platform(std::shared_ptr<DisplayReport> const& reporter);

    /* From Platform */
    std::shared_ptr<graphics::GraphicBufferAllocator> create_buffer_allocator() override;

    std::shared_ptr<graphics::Display> create_display(
        std::shared_ptr<DisplayConfigurationPolicy> const& initial_conf_policy,
        std::shared_ptr<GLProgramFactory> const& program_factory,
        std::shared_ptr<GLConfig> const& gl_config) override;

    std::shared_ptr<PlatformIpcOperations> make_ipc_operations() const override;

    EGLNativeDisplayType egl_native_display() const override;
private:
    Display *display;
    std::shared_ptr<mir::udev::Context> udev;
    std::shared_ptr<helpers::DRMHelper> const drm;
    helpers::GBMHelper gbm;
};

}
}
}
#endif /* MIR_GRAPHICS_X_PLATFORM_H_ */
