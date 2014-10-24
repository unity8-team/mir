/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#ifndef MIR_FRONTEND_SESSION_H_
#define MIR_FRONTEND_SESSION_H_

#include "mir_toolkit/common.h"
#include "mir/frontend/surface_id.h"

#include <memory>
#include <string>

namespace mir
{
namespace scene
{
struct SurfaceCreationParameters;
}

namespace graphics
{
class DisplayConfiguration;
}

namespace frontend
{
class Surface;

class Session
{
public:
    virtual ~Session() = default;

    virtual SurfaceId create_surface(scene::SurfaceCreationParameters const& params) = 0;
    virtual void destroy_surface(SurfaceId surface) = 0;
    virtual std::shared_ptr<Surface> get_surface(SurfaceId surface) const = 0;
    virtual std::shared_ptr<Surface> find_surface(SurfaceId surface) const = 0;

    virtual std::string name() const = 0;

    virtual void hide() = 0;
    virtual void show() = 0;

protected:
    Session() = default;
    Session(Session const&) = delete;
    Session& operator=(Session const&) = delete;
};

}
}

#endif // MIR_FRONTEND_SESSION_H_
