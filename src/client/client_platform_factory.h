/*
 * Copyright © 2012 Canonical Ltd.
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
#ifndef MIR_CLIENT_CLIENT_PLATFORM_FACTORY_H_
#define MIR_CLIENT_CLIENT_PLATFORM_FACTORY_H_

#include <mir_toolkit/common.h>
#include <memory>

namespace mir
{
namespace client
{

class ClientPlatform;
class ClientContext;

class ClientPlatformFactory
{
public:
    virtual ~ClientPlatformFactory() = default;

    virtual std::shared_ptr<ClientPlatform> create_client_platform(ClientContext* context) = 0;

protected:
    ClientPlatformFactory() = default;
    ClientPlatformFactory(ClientPlatformFactory const& p) = delete;
    ClientPlatformFactory& operator=(ClientPlatformFactory const& p) = delete;
};

extern "C" typedef std::shared_ptr<ClientPlatformFactory>(*CreateClientPlatformFactory)();
extern "C" MIR_API std::shared_ptr<ClientPlatformFactory> create_client_platform_factory();

}
}

#endif /* MIR_CLIENT_CLIENT_PLATFORM_FACTORY_H_ */
