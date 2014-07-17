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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_VISIBILITY_H_
#define MIR_VISIBILITY_H_

/*
 * Keep the attribute bit hidden in here; it's plausible that we might want to
 * support different compilers and such.
 *
 * Also, MIR_FOO is more descriptive
 */

#define MIR_INTERNAL __attribute__ ((visibility ("hidden")))
#define MIR_API __attribute__ ((visibility ("default")))

#endif
