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
#include "mir_test_doubles/mock_buffer_bundle.h"
#include "mir/compositor/buffer_handle.h"

#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace mtd = mir::test::doubles;

class BufferHandleTest : public ::testing::Test
{
protected:
    BufferHandleTest()
    {
        mock_buffer = std::make_shared<mtd::StubBuffer>();
        mock_bundle = std::make_shared<mtd::MockBufferBundle>();
    }

    std::shared_ptr<mtd::StubBuffer> mock_buffer;
    std::shared_ptr<mtd::MockBufferBundle> mock_bundle;
};

TEST_F(BufferHandleTest, can_contain_handout_and_release_compositor_buffer)
{
    using namespace testing;

    std::shared_ptr<mc::BufferHandle> buffer_handle =
        std::make_shared<mc::CompositorBufferHandle>(mock_bundle.get(), mock_buffer);

    EXPECT_CALL(*mock_bundle, compositor_release(_))
        .Times(1);

    ASSERT_EQ(buffer_handle->buffer(), mock_buffer);
}

TEST_F(BufferHandleTest, can_contain_handout_and_release_snapshot_buffer)
{
    using namespace testing;

    std::shared_ptr<mc::BufferHandle> buffer_handle =
        std::make_shared<mc::SnapshotBufferHandle>(mock_bundle.get(), mock_buffer);

    EXPECT_CALL(*mock_bundle, snapshot_release(_))
        .Times(1);

    ASSERT_EQ(buffer_handle->buffer(), mock_buffer);
}
