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
 */

#ifndef MIR_CLIENT_ENSURE_GLOBAL_SYMBOL_RESOLUTION_H
#define MIR_CLIENT_ENSURE_GLOBAL_SYMBOL_RESOLUTION_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Ensure the symbols defined by libmirclient are available for symbol
 * resolution in other shared objects.
 *
 * This is useful if libmirclient, or another library linking to libmirclient,
 * is loaded at runtime with dlopen() using RTLD_LOCAL (e.g., through a plugin
 * mechanism) and libmirclient's symbols need to be resolved at runtime in
 * other shared objects (like Mesa's libEGL).
 *
 */
void mir_client_ensure_global_symbol_resolution();

#ifdef __cplusplus
}
#endif

#endif /* MIR_CLIENT_ENSURE_GLOBAL_SYMBOL_RESOLUTION_H */
