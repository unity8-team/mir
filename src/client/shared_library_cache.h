/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_CLIENT_SHARED_LIBRARY_CACHE_H_
#define MIR_CLIENT_SHARED_LIBRARY_CACHE_H_

#include <memory>
#include <string>

namespace mir
{
class SharedLibrary;

namespace client
{

class SharedLibraryCache
{
public:
    virtual ~SharedLibraryCache() = default;

    virtual std::shared_ptr<mir::SharedLibrary>& library(std::string const& libname) = 0;

    static std::shared_ptr<SharedLibraryCache> instance();

protected:
    SharedLibraryCache() = default;
    SharedLibraryCache(SharedLibraryCache const&) = delete;
    SharedLibraryCache& operator=(SharedLibraryCache const&) = delete;
};

}
}

#endif
