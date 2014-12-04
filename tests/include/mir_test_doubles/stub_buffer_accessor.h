/*
 * Copyright © 2014 Canonical Ltd.
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
 *    Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_STUB_BUFFER_ACCESSOR_H_
#define MIR_TEST_DOUBLES_STUB_BUFFER_ACCESSOR_H_

#include "mir/graphics/buffer_accessor.h"

namespace mir
{
namespace test
{
namespace doubles
{

struct StubBufferAccessor : public graphics::BufferAccessor
{
    void write(graphics::Buffer& /* buffer */, unsigned char const* /* data */, size_t /* size */) override
    {
    }
    void read(graphics::Buffer& /* buffer */, std::function<void(unsigned char const*)> const& /* do_with_data */)
    {
    }
};

}
}
}

#endif // MIR_TEST_DOUBLES_STUB_BUFFER_ACCESSOR_H_
