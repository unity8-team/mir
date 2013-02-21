/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/server_configuration.h"
#include "protobuf_binder_communicator.h"

namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mc = mir::compositor;

std::shared_ptr<mf::Communicator>
mir::DefaultServerConfiguration::the_communicator(
    std::shared_ptr<sessions::SessionStore> const& session_store,
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mc::GraphicBufferAllocator> const& allocator)
{
    return communicator(
        []() -> std::shared_ptr<mf::Communicator>
        {
            auto const threads = the_options()->get("ipc_thread_pool", 10);
            return std::make_shared<mf::ProtobufBinderCommunicator>(
                the_socket_file(), the_ipc_factory(session_store, display, allocator));
        });

}
