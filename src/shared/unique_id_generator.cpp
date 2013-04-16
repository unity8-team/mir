/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir/unique_id_generator.h"
#include <stdexcept>

using namespace mir;

UniqueIdGenerator::UniqueIdGenerator(Validator validator, Id min, Id max)
    : min_id(min),
      max_id(max),
      is_valid(validator),
      next_id(min_id)
{
}

UniqueIdGenerator::~UniqueIdGenerator()
{
}

UniqueIdGenerator::Id UniqueIdGenerator::new_id()
{
    Id ret = next_id.fetch_add(1);
    int const range = max_id - min_id;
    int tries = 1;

    while (!is_valid(ret) || ret < min_id || ret > max_id)
    {
        tries++;
        if (tries > range)
            throw std::runtime_error("Exhausted UniqueIdGenerator");

        if (ret > max_id || ret < min_id)
            next_id.store(min_id);

        ret = next_id.fetch_add(1);
    }

    return ret;
}
