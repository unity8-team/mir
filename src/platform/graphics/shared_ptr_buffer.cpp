/*
 * Copyright © 2015 Canonical Ltd.
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

#include "mir/graphics/buffer.h"

/*
 * "Fully" instantiate the template here, in theory. Except it doesn't "fully"
 * work, because gcc forgets to instantiate the "= default" methods.
 *   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=51629
 *   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=57728
 *   https://gcc.gnu.org/bugzilla/show_bug.cgi?id=60796
 */
template class std::shared_ptr<mir::graphics::Buffer>;
