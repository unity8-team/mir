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

#ifndef MIR_SHELL_SURFACE_FACTORY_H_
#define MIR_SHELL_SURFACE_FACTORY_H_

#include <memory>

namespace mir
{
namespace scene { class SurfaceObserver; class Surface; }

namespace shell
{
class Session;
struct SurfaceCreationParameters;

class SurfaceFactory
{
public:
    virtual std::shared_ptr<scene::Surface> create_surface(
        Session* session,
        SurfaceCreationParameters const& params,
        std::shared_ptr<scene::SurfaceObserver> const& observer) = 0;

    virtual void destroy_surface(std::shared_ptr<scene::Surface> const& surface) = 0;

protected:
    virtual ~SurfaceFactory() {}
    SurfaceFactory() = default;
    SurfaceFactory(const SurfaceFactory&) = delete;
    SurfaceFactory& operator=(const SurfaceFactory&) = delete;
};
}
}

#endif // MIR_SHELL_SURFACE_FACTORY_H_
