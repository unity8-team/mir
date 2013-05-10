/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/server/graphics/android/interpreter_cache.h"
#include "mir_test_doubles/stub_buffer.h"

#include <gtest/gtest.h>
#include <stdexcept>

namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;

struct InterpreterResourceTest : public ::testing::Test
{
    void SetUp()
    {
        stub_buffer1 = std::make_shared<mtd::StubBuffer>();
        stub_buffer2 = std::make_shared<mtd::StubBuffer>();
        stub_buffer3 = std::make_shared<mtd::StubBuffer>();
        key1 = (ANativeWindowBuffer*)0x1;
        key2 = (ANativeWindowBuffer*)0x2;
        key3 = (ANativeWindowBuffer*)0x3;
    }

    std::shared_ptr<mtd::StubBuffer> stub_buffer1;
    std::shared_ptr<mtd::StubBuffer> stub_buffer2;
    std::shared_ptr<mtd::StubBuffer> stub_buffer3;
    ANativeWindowBuffer *key1;
    ANativeWindowBuffer *key2;
    ANativeWindowBuffer *key3; 
};

TEST_F(InterpreterResourceTest, deposit_buffer)
{
    mga::InterpreterCache cache;
    cache.store_buffer(stub_buffer1, key1);

    auto test_buffer = cache.retrieve_buffer(key1);
    EXPECT_EQ(stub_buffer1, test_buffer);
}

TEST_F(InterpreterResourceTest, deposit_many_buffers)
{
    mga::InterpreterCache cache;
    cache.store_buffer(stub_buffer1, key1);
    cache.store_buffer(stub_buffer2, key2);
    cache.store_buffer(stub_buffer3, key3);

    EXPECT_EQ(stub_buffer3, cache.retrieve_buffer(key3));
    EXPECT_EQ(stub_buffer1, cache.retrieve_buffer(key1));
    EXPECT_EQ(stub_buffer2, cache.retrieve_buffer(key2));
}

TEST_F(InterpreterResourceTest, deposit_buffer_has_ownership)
{
    mga::InterpreterCache cache;

    auto use_count_before = stub_buffer1.use_count();
    cache.store_buffer(stub_buffer1, key1);
    EXPECT_EQ(use_count_before+1, stub_buffer1.use_count());
    cache.retrieve_buffer(key1);
    EXPECT_EQ(use_count_before, stub_buffer1.use_count());
}

TEST_F(InterpreterResourceTest, retreive_buffer_with_bad_key_throws)
{
    mga::InterpreterCache cache;
    EXPECT_THROW({
        cache.retrieve_buffer(key1);
    }, std::runtime_error);
}
