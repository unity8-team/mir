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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "application_instance_mirclient_priv.h"

namespace uamc = ubuntu::application::mir::client;

uamc::Instance::Instance()
    : con(nullptr),
      ref_count(1),
      rpc_waiting_reply(false)
{
    
}

UApplicationInstance* uamc::Instance::as_u_application_instance()
{
    return static_cast<UApplicationInstance*>(this);
}

uamc::Instance* uamc::Instance::from_u_application_instance(UApplicationInstance* u_instance)
{
    return static_cast<uamc::Instance*>(u_instance);
}

void uamc::Instance::ref()
{
    ref_count++;
}

void uamc::Instance::unref()
{
    ref_count--;
    if (ref_count == 0)
        delete this;
}

MirConnection* uamc::Instance::connection() const
{
    return con.get();
}

bool uamc::Instance::connect(std::string const& application_name)
{
    auto mir_connection = mir_connect_sync(NULL, application_name.c_str());

    // mir_connect_sync() always returns a connection object, even in case
    // of a failed connection. We need to release this object in all cases.
    con = ConnectionPtr(mir_connection,
        [](MirConnection *c)
        {
            mir_connection_release(c);
        });

    return mir_connection_is_valid(mir_connection);
}
