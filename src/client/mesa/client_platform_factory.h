/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_CLIENT_MESA_CLIENT_PLATFORM_FACTORY_H_
#define MIR_CLIENT_MESA_CLIENT_PLATFORM_FACTORY_H_

#include "../client_platform_factory.h"

namespace mir
{
namespace client
{
namespace mesa
{

class ClientPlatformFactory : public client::ClientPlatformFactory
{
public:
    std::shared_ptr<ClientPlatform> create_client_platform(ClientContext* context) override;
};

}
}
}

#endif /* MIR_CLIENT_MESA_CLIENT_PLATFORM_FACTORY_H_ */