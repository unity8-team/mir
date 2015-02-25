/*
 * Copyright © 2015 Canonical Ltd.
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
 * Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#ifndef MIR_COMPOSITOR_BUFFER_HANDLE_H_
#define MIR_COMPOSITOR_BUFFER_HANDLE_H_

#include <memory>

namespace mir
{
namespace graphics { class Buffer; }

namespace compositor
{
class BufferBundle;

class BufferHandle
{
public:
    std::shared_ptr<graphics::Buffer> get_buffer();

protected:
    explicit BufferHandle(BufferBundle* bundle,
                 std::shared_ptr<graphics::Buffer> buffer);
    BufferBundle* buffer_bundle;
    std::shared_ptr<graphics::Buffer> buffer;
};

class CompositorBufferHandle : public BufferHandle
{
public:
	explicit CompositorBufferHandle(BufferBundle* bundle,
                 std::shared_ptr<graphics::Buffer> buffer);
    virtual ~CompositorBufferHandle();
};

class SnapshotBufferHandle : public BufferHandle
{
public:
	explicit SnapshotBufferHandle(BufferBundle* bundle,
                 std::shared_ptr<graphics::Buffer> buffer);
    virtual ~SnapshotBufferHandle();
};

}
}

#endif /*MIR_COMPOSITOR_BUFFER_HANDLE_H_*/
