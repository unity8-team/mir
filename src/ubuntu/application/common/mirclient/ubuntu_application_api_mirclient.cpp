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
#include "mircommon/lifecycle_delegate_mir_priv.h"
#include "mircommon/application_id_mir_priv.h"

#include <ubuntu/application/lifecycle_delegate.h>
#include <ubuntu/application/sensors/accelerometer.h>
#include <ubuntu/application/sensors/proximity.h>
#include <ubuntu/application/sensors/light.h>

#include <mir_toolkit/mir_client_library.h>

#include <stdlib.h>
#include <assert.h>
#include <string.h>

namespace uam = ubuntu::application::mir;
namespace uamc = uam::client;

namespace
{

static void dispatch_callback(MirConnection* conn, MirLifecycleState state, void* context)
{
    auto delegate = static_cast<uam::LifecycleDelegate*>(context);
    void* c_ctx = u_application_lifecycle_delegate_get_context(delegate->as_u_lifecycle_delegate(), nullptr);

    switch (state)
    {
    case mir_lifecycle_state_will_suspend:
    {
        if (delegate->stop_cb)
            delegate->stop_cb(nullptr, c_ctx);
        break;
    }
    case mir_lifecycle_state_resumed:
    {
        if (delegate->resumed_cb)
            delegate->resumed_cb(nullptr, c_ctx);
        break;
    }
    default:
        break;
    }
}
};

extern "C"
{

void u_application_init(void *args)
{
    (void) args;
}

void u_application_finish()
{
}

// TODO<papi>: Eliminate global instance by adding Instance to some functions (i.e. display queries)
// TODO: We can't take ownership of description and options until we get rid of the global instance
// so we leak them.
UApplicationInstance* u_application_instance_new_from_description_with_options(UApplicationDescription* description, UApplicationOptions* options)
{
    auto instance = global_mir_instance();

    auto id = uam::Id::from_u_application_id(u_application_description_get_application_id(description));
    auto connect_succeeded = instance->connect(id->name);

    if (!connect_succeeded)
        return nullptr;

    auto delegate = u_application_description_get_application_lifecycle_delegate(description);
    mir_connection_set_lifecycle_event_callback(instance->connection(), &dispatch_callback, delegate);

    return instance->as_u_application_instance();
}

void
u_application_instance_ref(UApplicationInstance *u_instance)
{
    auto instance = uamc::Instance::from_u_application_instance(u_instance);
    instance->ref();
}
    
void
u_application_instance_unref(UApplicationInstance *u_instance)
{
    auto instance = uamc::Instance::from_u_application_instance(u_instance);
    instance->unref();
}
    
void
u_application_instance_destroy(UApplicationInstance *u_instance)
{
    // TODO<papi>: What are the proper semantics here.
    u_application_instance_unref(u_instance);
}
    
void
u_application_instance_run(UApplicationInstance *instance)
{
    // TODO<papi>: What is this supposed to do? Seems to be no-op on hybris.
    (void) instance;
}

MirConnection*
u_application_instance_get_mir_connection(UApplicationInstance *u_instance)
{
    auto instance = uamc::Instance::from_u_application_instance(u_instance);
    return instance->connection();
}

}
