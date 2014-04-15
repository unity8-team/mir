/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#ifndef MIR_FRONTEND_SERVER_H_
#define MIR_FRONTEND_SERVER_H_

#include "mir/frontend/surface_id.h"

#include <sys/types.h>

#include <memory>

namespace mir
{
namespace scene
{
struct SurfaceCreationParameters;
}
namespace frontend
{
class EventSink;
class Session;

class Server
{
public:
    virtual ~Server() = default;

    virtual std::shared_ptr<Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<EventSink> const& sink) = 0;

    virtual void close_session(std::shared_ptr<Session> const& session)  = 0;

    virtual SurfaceId create_surface_for(
        std::shared_ptr<Session> const& session,
        scene::SurfaceCreationParameters const& params) = 0;

    virtual void handle_surface_created(std::shared_ptr<Session> const& session) = 0;

protected:
    Server() = default;
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;
};

}
}

#endif // MIR_FRONTEND_SERVER_H_
