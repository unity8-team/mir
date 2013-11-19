/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_SHELL_SESSION_CONTAINER_H_
#define MIR_SHELL_SESSION_CONTAINER_H_

#include <vector>
#include <memory>
#include <mutex>

namespace mir
{
namespace shell
{

class Session;

class SessionContainer
{
public:
    virtual void for_each(std::function<void(std::shared_ptr<Session> const&)> f) const = 0;

protected:
    SessionContainer() = default;
    virtual ~SessionContainer() = default;

    SessionContainer(const SessionContainer&) = delete;
    SessionContainer& operator=(const SessionContainer&) = delete;
};

}
}


#endif // MIR_SHELL_SESSION_CONTAINER_H_
