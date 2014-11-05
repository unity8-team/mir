/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_test_framework/using_stub_client_platform.h"
#include "mir_test_framework/stub_client_connection_configuration.h"
#include "mir_toolkit/mir_client_library.h"
#include "src/client/mir_connection_api.h"

#include <functional>

namespace mtf = mir_test_framework;
namespace mcl = mir::client;

namespace
{
class StubMirConnectionAPI : public mcl::MirConnectionAPI
{
public:
    StubMirConnectionAPI(mcl::MirConnectionAPI* prev_api)
        : prev_api{prev_api}
    {
        create_configuration = [&](std::string const& socket)
        {
            return std::unique_ptr<mcl::ConnectionConfiguration>(new mtf::StubConnectionConfiguration{socket});
        };
    }
    
    StubMirConnectionAPI(mcl::MirConnectionAPI* prev_api,
        std::function<std::unique_ptr<mcl::ConnectionConfiguration>(std::string const&)> const& create_configuration)
        : prev_api{prev_api},
          create_configuration{create_configuration}
    {
    }

    MirWaitHandle* connect(
        mcl::ConnectionConfiguration &conf,
        char const* socket_file,
        char const* name,
        mir_connected_callback callback,
        void* context) override
    {
        return prev_api->connect(conf, socket_file, name, callback, context);
    }

    void release(MirConnection* connection) override
    {
        return prev_api->release(connection);
    }

    std::unique_ptr<mcl::ConnectionConfiguration> configuration(std::string const& socket) override
    {
        return create_configuration(socket);
    }
    
private:
    mcl::MirConnectionAPI* const prev_api;
    std::function<std::unique_ptr<mcl::ConnectionConfiguration>(std::string const&)> create_configuration;
};

}

mtf::UsingStubClientPlatform::UsingStubClientPlatform()
    : prev_api{mir_connection_api_impl},
      stub_api{new StubMirConnectionAPI{prev_api}}
{
    mir_connection_api_impl = stub_api.get();
}

mtf::UsingStubClientPlatform::UsingStubClientPlatform(std::function<std::unique_ptr<mcl::ConnectionConfiguration>(std::string const&)>
     const& create_connection_configuration)
    : prev_api{mir_connection_api_impl},
      stub_api{new StubMirConnectionAPI{prev_api, create_connection_configuration}}
{
    mir_connection_api_impl = stub_api.get();
}

mtf::UsingStubClientPlatform::~UsingStubClientPlatform()
{
    mir_connection_api_impl = prev_api;
}
