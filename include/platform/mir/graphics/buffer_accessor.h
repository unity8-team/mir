/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_GRAPHICS_BUFFER_ACCESSOR_H_
#define MIR_GRAPHICS_BUFFER_ACCESSOR_H_

#include <cstddef>
#include <functional>

namespace mir
{
namespace graphics
{
class Buffer;

/// An interface provided by the graphics platform allowing for writing and reading untiled pixel data to and from buffers.
class BufferAccessor
{
public:
    virtual ~BufferAccessor() = default;

    // Expects data to be an unstrided array containing (buffer.width * buffer.height) pixels. Likewise
    // it is expected that buffer and data match in pixel format.
    virtual void write(Buffer& buffer, unsigned char const* data, size_t size) = 0;
    
    virtual void read(Buffer& buffer, std::function<void(unsigned char const*)> const& do_with_data) = 0;

protected:
    BufferAccessor() = default;
    BufferAccessor(BufferAccessor const&) = delete;
    BufferAccessor& operator=(BufferAccessor const&) = delete;
};

}
}

#endif // MIR_GRAPHICS_BUFFER_ACCESSOR_H_
