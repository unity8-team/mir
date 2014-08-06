/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "mir/default_server_configuration.h"
#include "mir/run_mir.h"
#include "mir/geometry/rectangle.h"

using namespace mir;
using namespace mir::geometry;

int main(int argc, char const* argv[])
{
    // Exercise some symbols from libmircommon. Make sure users of
    // mirserver.pc get these automatically in their link lines...
    Rectangle a;
    (void)a.bottom_right();
    Rectangle b;
    (void)a.contains(b);

    DefaultServerConfiguration config(argc, argv);
    run_mir(config, [](mir::DisplayServer&){} );
    return 0;
}
