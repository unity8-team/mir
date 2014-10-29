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

#ifndef MIR_TEST_FRAMEWORK_USING_STUB_CLIENT_PLATFORM_H_
#define MIR_TEST_FRAMEWORK_USING_STUB_CLIENT_PLATFORM_H_

#include "src/client/connection_configuration.h"

#include <memory>

namespace mir
{
namespace client
{
class MirConnectionAPI;
}
}

namespace mir_test_framework
{

class UsingStubClientPlatform
{
public:
    UsingStubClientPlatform();
    UsingStubClientPlatform(std::function<std::unique_ptr<mir::client::ConnectionConfiguration>(std::string const&)>
        const& create_connection_configuration);
    ~UsingStubClientPlatform();

private:
    UsingStubClientPlatform(UsingStubClientPlatform const&) = delete;
    UsingStubClientPlatform operator=(UsingStubClientPlatform const&) = delete;

    mir::client::MirConnectionAPI* prev_api;
    std::unique_ptr<mir::client::MirConnectionAPI> stub_api;
};

}

#endif /* MIR_TEST_FRAMEWORK_USING_STUB_CLIENT_PLATFORM_H_ */
