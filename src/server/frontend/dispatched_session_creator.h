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
 * Authored by: Christopher Halse Rogers <christopher.halse.rogers@canonical.com>
 */


#ifndef MIR_FRONTEND_DISPATCHED_SESSION_CREATOR_H_
#define MIR_FRONTEND_DISPATCHED_SESSION_CREATOR_H_

#include <memory>
#include <vector>
#include <boost/asio.hpp>
#include <uuid/uuid.h>

#include "mir/frontend/session_authorizer.h"
#include "mir/frontend/session_creator.h"

namespace mir
{
namespace frontend
{
class DispatchedSessionCreator
{
public:
    DispatchedSessionCreator() = default;
    virtual ~DispatchedSessionCreator() = default;

    virtual void create_session_for(std::shared_ptr<boost::asio::local::stream_protocol::socket> const& socket,
                                    std::string const& connection_data) = 0;

    virtual void protocol_id(uuid_t id) const = 0;
    virtual size_t header_size() const = 0;
private:
    DispatchedSessionCreator(DispatchedSessionCreator const&) = delete;
    DispatchedSessionCreator& operator=(DispatchedSessionCreator const&) = delete;
};
}
}

#endif // MIR_FRONTEND_DISPATCHED_SESSION_CREATOR_H_
