/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Cemil Azizoglu <cemil.azizoglu@canonical.com>
 */

#include "mir_test_doubles/stub_buffer.h"
#include "mir/compositor/buffer_handle.h"
#include "mir_test/fake_shared.h"

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

TEST(BufferHandleTest, can_contain_handout_and_release_buffer)
{
    mtd::StubBuffer mock_buffer;

    mc::BufferHandle buffer_handle(
                         mt::fake_shared(mock_buffer),
                         [&](mg::Buffer* b)
                         {
                             ASSERT_EQ(b, &mock_buffer);
                         });

    ASSERT_EQ(buffer_handle.buffer().get(), &mock_buffer);
}
