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

#include "application_id_mir_priv.h"

namespace uam = ubuntu::application::mir;

uam::Id::Id(const char *name, size_t size)
    : name(name, size)
{
}

UApplicationId* uam::Id::as_u_application_id()
{
    return static_cast<UApplicationId*>(this);
}

uam::Id* uam::Id::from_u_application_id(UApplicationId *id)
{
    return static_cast<uam::Id*>(id);
}

UApplicationId*
u_application_id_new_from_stringn(const char *string, size_t size)
{
    auto id = new uam::Id(string, size);
    return id->as_u_application_id();
}

void u_application_id_destroy(UApplicationId *u_id)
{
    auto id = uam::Id::from_u_application_id(u_id);
    delete id;
}

int u_application_id_compare(UApplicationId* u_lhs, UApplicationId* u_rhs)
{
    auto lhs = uam::Id::from_u_application_id(u_lhs);
    auto rhs = uam::Id::from_u_application_id(u_rhs);
    return lhs->name.compare(rhs->name);
}
