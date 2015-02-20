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
    BufferHandle(BufferBundle* bundle,
                 std::shared_ptr<graphics::Buffer> const& buffer);
    std::shared_ptr<graphics::Buffer> const& get_buffer();
    virtual ~BufferHandle() noexcept;

private:
    BufferBundle* buffer_bundle;
    std::shared_ptr<graphics::Buffer> const& buffer;
};
}
}

#endif /*MIR_COMPOSITOR_BUFFER_HANDLE_H_*/
