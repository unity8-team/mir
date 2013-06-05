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

#include <ubuntu/application/id.h>

#include <string>

namespace
{
struct ApplicationId
{
    ApplicationId(const char *name, size_t size)
        : name(name, size)
    {
    }
    std::string name;
};
}

UApplicationId*
u_application_id_new_from_stringn(const char *string, size_t size)
{
    return static_cast<UApplicationId*>(new ApplicationId(string, size));
}

void u_application_id_destroy(UApplicationId *u_id)
{
    auto id = static_cast<ApplicationId*>(u_id);
    delete id;
}

int u_application_id_compare(UApplicationId* u_lhs, UApplicationId* u_rhs)
{
    auto lhs = static_cast<ApplicationId*>(u_lhs);
    auto rhs = static_cast<ApplicationId*>(u_rhs);
    return lhs->name.compare(rhs->name);
}
