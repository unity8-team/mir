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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_SHARED_LIBRARY_PROBER_H_
#define MIR_SHARED_LIBRARY_PROBER_H_

#include <string>
#include <list>
#include <memory>

namespace mir
{
class SharedLibrary;

std::list<std::shared_ptr<SharedLibrary>> libraries_for_path(std::string const& path);
}


#endif /* MIR_SHARED_LIBRARY_PROBER_H_ */
