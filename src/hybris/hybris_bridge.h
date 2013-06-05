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

#ifndef UBUNTU_APPLICATION_API_HYBRIS_BRIDGE_H_
#define UBUNTU_APPLICATION_API_HYBRIS_BRIDGE_H_

namespace ubuntu
{
namespace hybris
{

struct Bridge
{
    Bridge();
    ~Bridge();

    static const char* path_to_library();
    static Bridge& instance();
    
    void* resolve_symbol(const char* symbol) const;

    void* lib_handle;
};

}
}

#endif // UBUNTU_APPLICATION_API_HYBRIS_BRIDGE_H_
