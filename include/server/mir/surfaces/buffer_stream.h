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
 * Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_SURFACES_BUFFER_STREAM_H_
#define MIR_SURFACES_BUFFER_STREAM_H_

#include "mir/geometry/size.h"
#include "mir/geometry/pixel_format.h"
#include "mir/graphics/buffer_id.h"

#include <memory>

namespace mir
{
namespace graphics
{
class Buffer;
}

namespace surfaces
{

class BufferStream
{
public:
    virtual ~BufferStream() {/* TODO: make nothrow */}

    virtual std::shared_ptr<graphics::Buffer> secure_client_buffer() = 0;
    virtual std::shared_ptr<graphics::Buffer> lock_compositor_buffer() = 0;
    virtual std::shared_ptr<graphics::Buffer> lock_snapshot_buffer() = 0;
    virtual geometry::PixelFormat get_stream_pixel_format() = 0;
    virtual geometry::Size stream_size() = 0;
    virtual void allow_framedropping(bool) = 0;
    virtual void force_requests_to_complete() = 0;
};

}
}

#endif /* MIR_SURFACES_BUFFER_STREAM_H_ */
