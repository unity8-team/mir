/*
 * Copyright (C) 2012 Canonical Ltd
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 *              Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */
#ifndef BASE_MODULE_H_
#define BASE_MODULE_H_

#include <bridge.h>

/*
 * This is the base backend loader for the Platform API
 */

namespace internal
{
/* Programs can select a backend with $UBUNTU_PLATFORM_API_BACKEND,
 * which either needs to be a full path or just the file name (then it will be
 * looked up in the usual library search path, see dlopen(3)).
 */
struct HIDDEN_SYMBOL ToBackend
{
    static const char* path()
    {
        static const char* cache = NULL;
        static char path[64];

        if (cache == NULL) {
            cache = secure_getenv("UBUNTU_PLATFORM_API_BACKEND");
            if (cache == NULL) {
                printf("UBUNTU PLATFORM API BACKEND NOT SELECTED -- Aborting\n");
                abort();
            } else {
                strcpy(path, "libubuntu_application_api_");
                if (strlen(cache) > MAX_MODULE_NAME) {
                    printf("Invalid Ubuntu Application API backend\n");
                    abort();
                }
                strcat(path, cache);
                strcat(path, ".so.2");
            }

        }

        return path;
    }

    static void* dlopen_fn(const char* path, int flags)
    {
        return dlopen(path, flags);
    }

    static void* dlsym_fn(void* handle, const char* symbol)
    {
        return dlsym(handle, symbol);
    }
};
}

#define DLSYM(fptr, sym) if (*(fptr) == NULL) { *((void**)fptr) = (void *) internal::Bridge<internal::ToBackend>::instance().resolve_symbol(sym); }

#include <bridge_defs.h>

#endif // BASE_MODULE_H_
