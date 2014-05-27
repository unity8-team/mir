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
#ifndef ANDROID_HW_MODULE_H_
#define ANDROID_HW_MODULE_H_

#include <bridge.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void *android_dlopen(const char *filename, int flag);
extern void *android_dlsym(void *handle, const char *symbol);

#ifdef __cplusplus
}
#endif

namespace internal
{
struct HIDDEN_SYMBOL ToHybris
{
    static const char* path()
    {
        static const char* cache = "/system/lib/libubuntu_application_api.so";
        return cache;
    }

    static void* dlopen_fn(const char* path, int flags)
    {
        return android_dlopen(path, flags);
    }

    static void* dlsym_fn(void* handle, const char* symbol)
    {
        return android_dlsym(handle, symbol);
    }
};
}

#define DLSYM(fptr, sym) if (*(fptr) == NULL) { *((void**)fptr) = (void *) internal::Bridge<internal::ToHybris>::instance().resolve_symbol(sym); }

#include <bridge_defs.h>

#endif // ANDROID_HW_MODULE_H_
