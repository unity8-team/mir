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

#include <ubuntu/application/description.h>

namespace
{
struct ApplicationDescription
{
    ApplicationDescription()
        : application_id(nullptr), lifecycle_delegate(nullptr)
    {
    }

    // TODO: Does the application description own this id?
    UApplicationId *application_id;
    // TODO: Likewise do we own the lifecycle delegate?
    UApplicationLifecycleDelegate *lifecycle_delegate;
};
}

UApplicationDescription*
u_application_description_new()
{
    return static_cast<UApplicationDescription*>(new ApplicationDescription);
}
    
void
u_application_description_destroy(UApplicationDescription *desc)
{
    auto d = static_cast<ApplicationDescription*>(desc);
    delete d;
}
    
void
u_application_description_set_application_id(UApplicationDescription *desc,
    UApplicationId *id)
{
    auto d = static_cast<ApplicationDescription*>(desc);
    d->application_id = id;
}
    
UApplicationId* 
u_application_description_get_application_id(UApplicationDescription *desc)
{
    auto d = static_cast<ApplicationDescription*>(desc);
    return d->application_id;
}
    
void
u_application_description_set_application_lifecycle_delegate(UApplicationDescription *desc,
    UApplicationLifecycleDelegate *lifecycle_delegate)
{
    auto d = static_cast<ApplicationDescription*>(desc);
    d->lifecycle_delegate = lifecycle_delegate;
}
    
UApplicationLifecycleDelegate* 
u_application_description_get_application_lifecycle_delegate(UApplicationDescription *desc)
{
    auto d = static_cast<ApplicationDescription*>(desc);
    return d->lifecycle_delegate;
}
