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
#ifndef BASE_BRIDGE_H_
#define BASE_BRIDGE_H_

#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_MODULE_NAME 32

#define HIDDEN_SYMBOL __attribute__ ((visibility ("hidden")))

namespace internal
{
template<typename Scope>
class HIDDEN_SYMBOL Bridge
{
  public:
    static Bridge<Scope>& instance()
    { 
        static Bridge<Scope> bridge; 
        return bridge; 
    }

    void* resolve_symbol(const char* symbol) const
    {
        return Scope::dlsym_fn(lib_handle, symbol);
    }

  protected:
    Bridge() : lib_handle(Scope::dlopen_fn(Scope::path(), RTLD_LAZY))
    {
    }

    ~Bridge()
    {
    }

    void* lib_handle;
};
}

#endif // BRIDGE_H_
