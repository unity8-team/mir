/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_PROCESS_SPAWNER_H_
#define MIR_PROCESS_SPAWNER_H_

#include <memory>
#include <initializer_list>

namespace mir
{
namespace process
{

class Handle;

class Spawner
{
public:
    virtual ~Spawner() = default;
    virtual std::shared_ptr<Handle> run_from_path(char const* binary_name) const = 0;
    virtual std::shared_ptr<Handle> run_from_path(char const* binary_name, std::initializer_list<char const*> args) const = 0;
};
}
}
#endif  // MIR_PROCESS_SPAWNER_H_
