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

#include "shared_library_cache.h"
#include "mir/cached_ptr.h"

#include <map>

namespace mcl = mir::client;

namespace
{

class MapSharedLibraryCache : public mcl::SharedLibraryCache
{
public:
    std::shared_ptr<mir::SharedLibrary>& library(std::string const& libname) override
    {
        return cache[libname];
    }

private:
    std::map<std::string, std::shared_ptr<mir::SharedLibrary>> cache;
};

}

std::shared_ptr<mcl::SharedLibraryCache> mcl::SharedLibraryCache::instance()
{
    static mir::CachedPtr<mcl::SharedLibraryCache> cache;

    return cache([] { return std::make_shared<MapSharedLibraryCache>();});
}
