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

#ifndef MIR_UNIQUE_ID_GENERATOR_H_
#define MIR_UNIQUE_ID_GENERATOR_H_

#include <atomic>
#include <limits>
#include <functional>

namespace mir
{

class UniqueIdGenerator
{
public:
    typedef int Id;  // Should always remain int compatible.
    typedef std::function<bool(Id)> Validator;

    UniqueIdGenerator(Validator validator,
                      Id error = 0,
                      Id min = 1,
                      Id max = std::numeric_limits<Id>::max());
    virtual ~UniqueIdGenerator();

    Id new_id();

    Id const min_id, max_id, invalid_id;

private:
    Validator const is_valid;
    std::atomic<Id> next_id;
};

} // namespace mir

#endif // MIR_UNIQUE_ID_GENERATOR_H_
