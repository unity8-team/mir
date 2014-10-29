
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

#include "mir/shared_library.h"

#include "mir/geometry/rectangle.h"

#include "mir_test_framework/platform_loader_helpers.h"
#include "mir_test_framework/stub_server_platform_factory.h"

#include <vector>

namespace geom = mir::geometry;
namespace mg = mir::graphics;
namespace mtf = mir_test_framework;

namespace
{
// NOTE: Raw pointer, deliberately leaked to bypass all the fun
//       issues around global destructor ordering.
mir::SharedLibrary* platform_lib{nullptr};

void ensure_platform_library()
{
    if (!platform_lib)
    {
        platform_lib = new mir::SharedLibrary{mtf::server_platform_stub_path()};
    }
}
}

std::shared_ptr<mg::Platform> mtf::make_stubbed_server_graphics_platform(std::vector<geom::Rectangle> const& display_rects)
{
    ensure_platform_library();
    auto factory = platform_lib->load_function<std::shared_ptr<mg::Platform>(*)(std::vector<geom::Rectangle> const&)>("create_stub_platform");

    return factory(display_rects);
}
