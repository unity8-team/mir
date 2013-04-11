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

#include "unique_id_generator.h"

using namespace mir::client;

UniqueIdGenerator::UniqueIdGenerator(Id error, Id min, Id max)
    : min_id(min),
      max_id(max),
      invalid_id(error),
      next_id(min_id)
{
}

UniqueIdGenerator::~UniqueIdGenerator()
{
}

UniqueIdGenerator::Id UniqueIdGenerator::new_id()
{
    Id id = next_id.fetch_add(1);
    int range = max_id - min_id + 1;
    int tries = 1;

    while (id == invalid_id || id_in_use(id) || id < min_id || id > max_id)
    {
        id = next_id.fetch_add(1);

        tries++;
        if (tries > range)
            return invalid_id;

        if (id > max_id || id < min_id)
            next_id.store(min_id);
    }

    return id;
}
