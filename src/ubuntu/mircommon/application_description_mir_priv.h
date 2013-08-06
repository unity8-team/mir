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

#ifndef UBUNTU_APPLICATION_DESCRIPTION_MIR_PRIV_H_
#define UBUNTU_APPLICATION_DESCRIPTION_MIR_PRIV_H_

#include <ubuntu/application/description.h>

#include <memory>
#include <functional>

namespace ubuntu
{
namespace application
{
namespace mir
{

class Description
{
public:
    Description();
    ~Description() = default;
    
    UApplicationDescription* as_u_application_description();
    static Description* from_u_application_description(UApplicationDescription* u_description);

    // TODO<papi> Do we really own these?
    std::unique_ptr<UApplicationId, std::function<void(UApplicationId*)>> application_id;
    std::unique_ptr<UApplicationLifecycleDelegate, std::function<void(UApplicationLifecycleDelegate*)>> lifecycle_delegate;

protected:
    Description(Description const&) = delete;
    Description& operator=(Description const&) = delete;
};

}
}
} // namespace ubuntu

#endif // UBUNTU_APPLICATION_DESCRIPTION_MIR_PRIV_H_
