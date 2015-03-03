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

class BufferBundle;

typedef std::function<void(graphics::Buffer* buffer)> release_callback;

class BufferHandle
{
public:
    explicit BufferHandle(BufferBundle* bundle,
        std::shared_ptr<graphics::Buffer> const& buffer,
        release_callback const& release);

    virtual ~BufferHandle();

    std::shared_ptr<graphics::Buffer> buffer();

    BufferHandle(BufferHandle&& other);
    BufferHandle& operator=(BufferHandle&& other);

private:
    BufferHandle(BufferHandle const&) = delete;
    BufferHandle& operator=(BufferHandle const&) = delete;

    BufferBundle* buffer_bundle;
    std::shared_ptr<graphics::Buffer> wrapped;
    release_callback const release_fn;
};

}
}

#endif /* MIR_BUFFER_HANDLE_H_*/
