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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#ifndef MIR_SHELL_SESSION_H_
#define MIR_SHELL_SESSION_H_

#include "mir/shell/surface_id.h"

#include <mutex>
#include <atomic>
#include <memory>
#include <string>

namespace mir
{

/// Management of client application shell
namespace shell
{
class Surface;
class SurfaceCreationParameters;

class Session
{
public:
    virtual ~Session() {}

    virtual SurfaceId create_surface(SurfaceCreationParameters const& params) = 0;
    virtual void destroy_surface(SurfaceId surface) = 0;
    virtual std::shared_ptr<Surface> get_surface(SurfaceId surface) const = 0;

    virtual std::string name() = 0;
    virtual void shutdown() = 0;

    virtual void hide() = 0;
    virtual void show() = 0;
    
    virtual bool has_appeared() const = 0;

protected:
    Session() = default;
    Session(Session const&) = delete;
    Session& operator=(Session const&) = delete;
};

}
}

#endif // MIR_SHELL_SESSION_H_
