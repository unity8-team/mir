/*
 * Copyright © 2014 Canonical Ltd.
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

#ifndef VSYNC_SIMULATING_GRAPHICS_PLATFORM_H_
#define VSYNC_SIMULATING_GRAPHICS_PLATFORM_H_

#include "mir/graphics/platform.h"
#include "mir/geometry/rectangle.h"

// TODO: Simulate screen geometry and vsync
class VsyncSimulatingPlatform : public mir::graphics::Platform
{
public:
    VsyncSimulatingPlatform() = default;
    ~VsyncSimulatingPlatform() = default;
    
    std::shared_ptr<mir::graphics::GraphicBufferAllocator> create_buffer_allocator(
        std::shared_ptr<mir::graphics::BufferInitializer> const& buffer_initializer);
    std::shared_ptr<mir::graphics::BufferWriter> make_buffer_writer();
    
    std::shared_ptr<mir::graphics::Display> create_display(
        std::shared_ptr<mir::graphics::DisplayConfigurationPolicy> const& initial_conf_policy,
        std::shared_ptr<mir::graphics::GLProgramFactory> const& gl_program_factory,
        std::shared_ptr<mir::graphics::GLConfig> const& gl_config);
    
    std::shared_ptr<mir::graphics::PlatformIpcOperations> make_ipc_operations() const;
    std::shared_ptr<mir::graphics::InternalClient> create_internal_client();
private:
    int stuff_will_go_here;
};

#endif // VSYNC_SIMULATING_GRAPHICS_PLATFORM_H_


