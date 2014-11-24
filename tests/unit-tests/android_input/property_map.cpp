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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#include <std/PropertyMap.h>

#include <fstream>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace android;

namespace
{
static char const* test_file = "AndroidInputPropertyMap.ini";

struct AndroidInputPropertyMap : public ::testing::Test
{
    PropertyMap* test_map;

    static void SetUpTestCase()
    {
        ASSERT_TRUE(std::ofstream(test_file) <<
            "test.string=a_string\n"
            "#test.bool.true=true\n"
            "test.bool.true=1\n"
            "test.bool.false=0\n"
            "#test.bool.true=true\n"
            "# a comment\n"
            "#test.int.ignored=1\n"
            "test.int_32=123\n"
            "test.float=0.5\n");
    }

    void SetUp()
    {
        ASSERT_EQ(NO_ERROR, PropertyMap::load(String8(test_file), &test_map));
    }

    void TearDown()
    {
        delete test_map;
    }
};
}

TEST_F(AndroidInputPropertyMap, TestMapCreated)
{
    EXPECT_TRUE(test_map);
}

TEST_F(AndroidInputPropertyMap, TestMapHasAString)
{
    String8 result;
    EXPECT_TRUE(test_map->tryGetProperty(String8("test.string"), result));
    EXPECT_EQ(String8("a_string"), result);
}

TEST_F(AndroidInputPropertyMap, TestMapHasAnInt32T)
{
    int32_t result;
    EXPECT_TRUE(test_map->tryGetProperty(String8("test.int_32"), result));
    EXPECT_EQ(123, result);
}

TEST_F(AndroidInputPropertyMap, TestMapHasBoolsTrueAndFalse)
{
    bool result{};
    EXPECT_TRUE(test_map->tryGetProperty(String8("test.bool.true"), result));
    EXPECT_TRUE(result);

    EXPECT_TRUE(test_map->tryGetProperty(String8("test.bool.false"), result));
    EXPECT_FALSE(result);
}

TEST_F(AndroidInputPropertyMap, TestMapHasAFloat)
{
    float result;
    EXPECT_TRUE(test_map->tryGetProperty(String8("test.float"), result));
    EXPECT_EQ(0.5, result);
}

TEST_F(AndroidInputPropertyMap, TestMapFailsToGetUnknownString)
{
    String8 result;
    EXPECT_FALSE(test_map->tryGetProperty(String8("unknown"), result));
}

TEST_F(AndroidInputPropertyMap, TestMapFailsToGetUnknownBool)
{
    bool result{};
    EXPECT_FALSE(test_map->tryGetProperty(String8("unknown"), result));
}

TEST_F(AndroidInputPropertyMap, TestMapFailsToGetUnknownInt32T)
{
    int32_t result;
    EXPECT_FALSE(test_map->tryGetProperty(String8("unknown"), result));
}

TEST_F(AndroidInputPropertyMap, TestMapFailsToGetUnknownFloat)
{
    float result;
    EXPECT_FALSE(test_map->tryGetProperty(String8("unknown"), result));
}

TEST_F(AndroidInputPropertyMap, TestMapIgnoresComment)
{
    int32_t result;
    EXPECT_FALSE(test_map->tryGetProperty(String8("test.int.ignored"), result));
}

