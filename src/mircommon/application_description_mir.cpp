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

#include "application_description_mir_priv.h"

namespace uam = ubuntu::application::mir;

uam::Description::Description()
    : application_id(nullptr),
      lifecycle_delegate(nullptr)
{
}

uam::Description::~Description()
{
    // TODO<papi>: Do we have any ownership responsibilities over the application_id or
    // lifecycle delegate
}

UApplicationDescription* uam::Description::as_u_application_description()
{
    return static_cast<UApplicationDescription*>(this);
}

uam::Description* uam::Description::from_u_application_description(UApplicationDescription* description)
{
    return static_cast<uam::Description*>(description);
}

UApplicationDescription*
u_application_description_new()
{
    auto description = new uam::Description();
    return description->as_u_application_description();
}
    
void
u_application_description_destroy(UApplicationDescription *u_description)
{
    auto description = uam::Description::from_u_application_description(u_description);
    delete description;
}
    
void
u_application_description_set_application_id(UApplicationDescription *u_description,
    UApplicationId *id)
{
    auto description = uam::Description::from_u_application_description(u_description);
    description->application_id = id;
}
    
UApplicationId* 
u_application_description_get_application_id(UApplicationDescription *u_description)
{
    auto description = uam::Description::from_u_application_description(u_description);
    return description->application_id;
}
    
void
u_application_description_set_application_lifecycle_delegate(UApplicationDescription *u_description,
    UApplicationLifecycleDelegate *lifecycle_delegate)
{
    auto description = uam::Description::from_u_application_description(u_description);
    description->lifecycle_delegate = lifecycle_delegate;
}
    
UApplicationLifecycleDelegate* 
u_application_description_get_application_lifecycle_delegate(UApplicationDescription *u_description)
{
    auto description = uam::Description::from_u_application_description(u_description);
    return description->lifecycle_delegate;
}
