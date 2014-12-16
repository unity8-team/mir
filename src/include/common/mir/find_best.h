/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Andeas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_FIND_BEST_H_
#define MIR_FIND_BEST_H_

#include <functional>

namespace mir
{

struct Greater // replace in favor of std::greater<> when we switch to c++14
{
    template<typename T>
    bool operator()(T const& l, T const& r) const
    {
        return l > r;
    }
};

/**
 * \brief Range based algorithm to find the best suited item.
 *
 * Use this instead of std::max_element or std::min_element, when
 * the comparision involves a costly test, as it caches the test result
 * of the individual best candidate. To acchieve that the predicate is
 * split in a predicate and a transformation.
 */
template<typename Container, typename Transform, typename Predicate = Greater>
auto find_best(Container const& items, Transform const& transform,
               decltype(transform(*items.begin())) const& initial_best,
               Predicate const& pred = Greater{})
-> decltype(begin(items))
{
    auto current_best_it = end(items);
    auto current_best = initial_best;
    for (auto it = begin(items), e = end(items);
         it != e; ++it)
    {
        auto item_transformed = transform(*it);

        if (pred(item_transformed, current_best))
        {
            current_best = item_transformed;
            current_best_it = it;
        }
    }
    return current_best_it;
}
}

#endif
