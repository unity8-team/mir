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
#include <stdio.h>
#include <unistd.h>

/*
 * This is the base backend loader for the Platform API
 */

#define API_VERSION_MAJOR   "2"
#define API_VERSION_MINOR   "2"
#define API_VERSION_PATCH   "0"
#define SO_SUFFIX ".so." API_VERSION_MAJOR "." API_VERSION_MINOR "." API_VERSION_PATCH

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
        static char* cache = NULL;
        static char path[64];
        char module_name[32];

        if (cache == NULL) {
            cache = secure_getenv("UBUNTU_PLATFORM_API_BACKEND");
            if (cache == NULL) {
                FILE *conf;
                conf = fopen("/etc/ubuntu-platform-api/application.conf", "r");
                if (conf == NULL) {
                    exit_module("Unable to find module configuration file");
                } else {
                    if (fgets(module_name, 32, conf))
                        cache = module_name;
                    else
                        exit_module("Error reading module name from file");
                }
                // Null terminate module blob
                cache[strlen(cache)-1] = '\0';
                fclose(conf);
            }
            if (cache == NULL)
                exit_module("Unable to determine backend");

            strcpy(path, "libubuntu_application_api_");
            
            if (strlen(cache) > MAX_MODULE_NAME)
                exit_module("Selected module is invalid");
            
            strcat(path, cache);
            strcat(path, SO_SUFFIX);
        }

        return path;
    }

    static void exit_module(const char* msg)
    {
        printf("Ubuntu Platform API: %s -- Aborting\n", msg);
        abort();
    }

    static void* dlopen_fn(const char* path, int flags)
    {
        void *handle = dlopen(path, flags);
        if (handle == NULL)
            exit_module("Unable to load selected module.");

        return handle;
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
