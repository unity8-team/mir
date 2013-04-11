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

#ifndef MIR_CLIENT_UNIQUE_ID_GENERATOR_H_
#define MIR_CLIENT_UNIQUE_ID_GENERATOR_H_

#include <atomic>
#include <climits>

namespace mir
{
namespace client
{

class UniqueIdGenerator
{
public:
    typedef int Id;  // Should always remain int compatible.

    UniqueIdGenerator(Id error = 0, Id min = 1, Id max = INT_MAX);
    virtual ~UniqueIdGenerator();

    virtual bool id_in_use(Id x) const = 0;

    Id new_id();

    Id const min_id, max_id, invalid_id;

private:
    std::atomic<Id> next_id;
};

} // namespace client
} // namespace mir

#endif // MIR_CLIENT_UNIQUE_ID_GENERATOR_H_
