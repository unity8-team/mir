/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by:
 *   Thomas Guest  <thomas.guest@canonical.com>
 */

#ifndef MIR_GRAPHICS_PLATFORM_H_
#define MIR_GRAPHICS_PLATFORM_H_

#include "mir/geometry/pixel_format.h"

#include <memory>
#include <vector>

namespace mir
{
namespace frontend
{
class Surface;
}
namespace compositor
{
class GraphicBufferAllocator;
class Buffer;
class BufferIPCPacker;
}

/// Graphics subsystem. Mediates interaction between core system and
/// the graphics environment.
namespace graphics
{

class Display;
struct PlatformIPCPackage;
class BufferInitializer;
class InternalClient;
class DisplayReport;

/// Interface to platform specific support for graphics operations.
class Platform
{
public:
    Platform() = default;
    Platform(const Platform& p) = delete;
    Platform& operator=(const Platform& p) = delete;

    virtual std::shared_ptr<compositor::GraphicBufferAllocator> create_buffer_allocator(
        std::shared_ptr<BufferInitializer> const& buffer_initializer) = 0;
    virtual std::shared_ptr<Display> create_display() = 0;
    virtual std::shared_ptr<PlatformIPCPackage> get_ipc_package() = 0;
    virtual void fill_ipc_package(std::shared_ptr<compositor::BufferIPCPacker> const& packer,
                                  std::shared_ptr<compositor::Buffer> const& buffer) const = 0;
    
    virtual std::shared_ptr<InternalClient> create_internal_client() = 0;
    virtual std::vector<geometry::PixelFormat> supported_pixel_formats() = 0;
};

// Create and return a new graphics platform.
std::shared_ptr<Platform> create_platform(std::shared_ptr<DisplayReport> const& report);

}
}

#endif // MIR_GRAPHICS_PLATFORM_H_
