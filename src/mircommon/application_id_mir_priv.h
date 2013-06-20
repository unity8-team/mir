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

#ifndef UBUNTU_APPLICATION_ID_MIR_PRIV_H_
#define UBUNTU_APPLICATION_ID_MIR_PRIV_H_

#include <ubuntu/application/id.h>

#include <string>

namespace ubuntu
{
namespace application
{
namespace mir
{

class Id
{
public:
    Id(const char *name, size_t size);
    ~Id() = default;
    
    UApplicationId* as_u_application_id();
    static Id* from_u_application_id(UApplicationId* id);

    std::string const name;

protected:
    Id(const Id&) = delete;
    Id& operator=(const Id&) = delete;
};

}
}
} // namespace ubuntu

#endif // UBUNTU_APPLICATION_ID_MIR_PRIV_H_
