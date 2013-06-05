/*
 * Copyright (C) 2013 Canonical Ltd
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

#include "hybris_bridge.h"

#include <dlfcn.h>
#include <assert.h>

namespace uh = ubuntu::hybris;

extern "C" {

extern void *android_dlopen(const char *filename, int flag);
extern void *android_dlsym(void *handle, const char *symbol);

}

static const char* uh::Bridge::path_to_library()
{
    return "/system/lib/libubuntu_application_api.so";
}

uh::Bridge& uh::Bridge::instance()
{
    static uh::Bridge bridge;
    return bridge;
}

uh::Bridge()
    : lib_handle(android_dlopen(path_to_library(), RTLD_LAZY))
{
    assert(lib_handle && "Error loading ubuntu_application_api");
}

uh::~Bridge()
{
    // TODO android_dlclose(libcamera_handle);
}

void* uh::Bridge::resolve_symbol(const char* symbol) const
{
    return android_dlsym(lib_handle, symbol);
}
