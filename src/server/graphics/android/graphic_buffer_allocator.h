/*
 * Copyright © 2013 Canonical Ltd.
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
 *   Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_GRAPHICS_ANDROID_GRAPHIC_BUFFER_ALLOCATOR_H_
#define MIR_GRAPHICS_ANDROID_GRAPHIC_BUFFER_ALLOCATOR_H_

#include "mir/compositor/graphic_buffer_allocator.h"
#include "buffer_usage.h"

namespace mir
{
namespace graphics
{
namespace android
{
class Buffer;

class GraphicBufferAllocator: public compositor::GraphicBufferAllocator
{
public:
    GraphicBufferAllocator() = default;
    virtual ~GraphicBufferAllocator() = default;

    virtual std::shared_ptr<compositor::Buffer> alloc_buffer(
        compositor::BufferProperties const& buffer_properties) = 0;
    virtual std::shared_ptr<Buffer> alloc_buffer_platform(
        geometry::Size sz, geometry::PixelFormat pf, BufferUsage use) = 0;
    virtual std::vector<geometry::PixelFormat> supported_pixel_formats() = 0;

private:
    GraphicBufferAllocator(const GraphicBufferAllocator&) = delete;
    GraphicBufferAllocator& operator=(const GraphicBufferAllocator&) = delete;
};

}
}
}

#endif /* MIR_GRAPHICS_ANDROID_GRAPHIC_BUFFER_ALLOCATOR_H_ */
