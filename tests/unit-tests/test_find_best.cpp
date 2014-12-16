/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR AStructure PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "mir/find_best.h"
#include <gtest/gtest.h>

#include <initializer_list>

namespace
{
struct AStructure
{
    int value;
};

int trivial_transform(AStructure const& a)
{
    return a.value;
}
}

TEST(FindBest, returns_end_iterator_on_empty)
{
    using namespace testing;
    std::initializer_list<::AStructure> empty{};
    EXPECT_THAT(mir::find_best(empty, trivial_transform, 0), Eq(end(empty)));
}

TEST(FindBest, returns_best_element)
{
    using namespace testing;
    std::vector<::AStructure> values{{123}, {23}, {500}, {42}};
    EXPECT_THAT(mir::find_best(values, trivial_transform, 0)->value, Eq(500));
}

TEST(FindBest, returns_end_when_initial_is_best)
{
    using namespace testing;
    std::vector<::AStructure> values{{123}, {23}, {500}, {42}};
    EXPECT_THAT(mir::find_best(values, trivial_transform, 501), Eq(end(values)));
}

TEST(FindBest, calls_transformation_only_n_times)
{
    using namespace testing;
    std::vector<::AStructure> values{{123}, {23}, {500}, {42}};
    int transform_count = 0;
    mir::find_best(values, [&transform_count](::AStructure const& item) { ++transform_count; return item.value; }, 0);
    EXPECT_THAT(transform_count, Eq(values.size()));
}

TEST(FindBest, different_predicates_can_be_used)
{
    using namespace testing;
    std::vector<::AStructure> values{{123}, {23}, {500}, {42}};
    EXPECT_THAT(mir::find_best(values, trivial_transform, 500, std::less<int>())->value, Eq(23));
}
