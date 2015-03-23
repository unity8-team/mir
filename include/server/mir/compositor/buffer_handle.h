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

#ifndef MIR_BUFFER_HANDLE_H_
#define MIR_BUFFER_HANDLE_H_

#include <memory>

namespace mir
{
namespace graphics { class Buffer; }

namespace compositor
{

typedef std::function<void(graphics::Buffer* buffer)> ReleaseCallback;

class BufferHandle
{
public:
    explicit BufferHandle(
        std::shared_ptr<graphics::Buffer> const& buffer,
        ReleaseCallback const& release);

    BufferHandle(BufferHandle&& other);
    BufferHandle& operator=(BufferHandle&& other);
    BufferHandle() = default;

    ~BufferHandle();

    std::shared_ptr<graphics::Buffer> buffer();
    bool operator!();

private:
    BufferHandle(BufferHandle const&) = delete;
    BufferHandle& operator=(BufferHandle const&) = delete;

    std::shared_ptr<graphics::Buffer> wrapped;
    ReleaseCallback release_fn;
};

}
}

#endif /* MIR_BUFFER_HANDLE_H_*/
