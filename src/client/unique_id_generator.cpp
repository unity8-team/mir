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

UniqueIdGenerator::UniqueIdGenerator(id_t error_value)
    : invalid_id(error_value),
      next_id(invalid_id + 1)
{
}

UniqueIdGenerator::~UniqueIdGenerator()
{
}

UniqueIdGenerator::id_t UniqueIdGenerator::new_id()
{
    id_t id;

    do
    {
        id = next_id.fetch_add(1);
    } while (id == invalid_id || id_in_use(id));

    return id;
}
