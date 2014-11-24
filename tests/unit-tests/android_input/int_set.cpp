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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

// Need a different implementation of IntSet with ANDROID_INPUT_INTSET_TEST
// defined. It will fall under test::android namespace instead of in android
// to avoid clashing with the production IntSet from libmirserver
#define ANDROID_INPUT_INTSET_TEST
#include <androidfw/IntSet.h>
#include <IntSet.cpp>

int test::android::IntSet::constructionCount;
int test::android::IntSet::destructionCount;

#include <gtest/gtest.h>

#include <string>
#include <vector>

using std::vector;
using std::string;

namespace
{
struct AndroidInputIntSet : public ::testing::Test
{
    void SetUp()
    {
    }
};
}

TEST_F(AndroidInputIntSet, Difference)
{
    IntSet a = {1, 2, 3,       6};
    IntSet b = {   2, 3, 4, 5};
    IntSet c = a - b;

    IntSet expected_c = {1, 6};
    EXPECT_EQ(expected_c, c);
}

TEST_F(AndroidInputIntSet, Intersection)
{
    IntSet a = {1, 2, 3};
    IntSet b = {   2, 3, 4, 5};
    IntSet c = a & b;

    IntSet expected_c = {2, 3};
    EXPECT_EQ(expected_c, c);
}

TEST_F(AndroidInputIntSet, First)
{
    IntSet a = {4, 2, 1, 3};
    EXPECT_EQ(1, a.first());
}

TEST_F(AndroidInputIntSet, RemoveSet)
{
    IntSet a = {1, 2, 3, 4};
    IntSet b = {   2, 3,    5};

    a.remove(b);

    IntSet expected_a = {1, 4};
    EXPECT_EQ(expected_a, a);
}

TEST_F(AndroidInputIntSet, IndexOf)
{
    IntSet a = {5, 6, 10, 15};

    EXPECT_EQ(1u, a.indexOf(6));
    EXPECT_EQ(2u, a.indexOf(10));
}

TEST_F(AndroidInputIntSet, ForEach)
{
    IntSet a = {5, 6, 10, 15};

    std::vector<int32_t> expected_values = {5, 6, 10, 15};
    std::vector<int32_t> actual_values;

    a.forEach([&](int32_t value) {
        actual_values.push_back(value);
    });

    EXPECT_EQ(expected_values, actual_values);
}

TEST_F(AndroidInputIntSet, ToString)
{
    IntSet a = {1, 2, 3};
    string expected_str = "1, 2, 3";
    EXPECT_EQ(expected_str, a.toString());
}

TEST_F(AndroidInputIntSet, UnnecessaryConstructions)
{
    IntSet a = {1, 2, 3,       6};
    IntSet b = {   2, 3, 4, 5};

    IntSet::constructionCount = 0;
    IntSet::destructionCount = 0;
    {
        IntSet c = a - b;
    }
    EXPECT_EQ(1, IntSet::constructionCount);
    EXPECT_EQ(1, IntSet::destructionCount);

    IntSet::constructionCount = 0;
    IntSet::destructionCount = 0;
    {
        IntSet c = a & b;
    }
    EXPECT_EQ(1, IntSet::constructionCount);
    EXPECT_EQ(1, IntSet::destructionCount);
}
