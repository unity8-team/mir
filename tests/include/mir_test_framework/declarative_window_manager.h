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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TEST_FRAMEWORK_DECLARATIVE_WINDOW_MANAGER_H_
#define MIR_TEST_FRAMEWORK_DECLARATIVE_WINDOW_MANAGER_H_

#include "mir/shell/window_manager.h"
#include "mir/geometry/rectangle.h"
#include "mir/scene/depth_id.h"

#include <memory>
#include <map>
#include <string>

namespace mir_test_framework
{
typedef std::map<std::string, mir::geometry::Rectangle> SurfaceGeometries;
typedef std::map<std::string, mir::scene::DepthId> SurfaceDepths;

/// DeclarativeWindowManager is a test utility server component for specifying
/// a static list of surface geometries and relative depths. Used, for example,
/// in input tests where it is necessary to set up scenarios depending on
/// multiple surfaces geometry and stacking.
class DeclarativeWindowManager : public mir::shell::WindowManager
{
 public:
    // Placement requests will be passed through to default strategy, and then overriden if the surface appears
    // in the geometry or depth map. This allows for the convenience of leaving some surfaces geometries unspecified
    // and receiving the default behavior.
    DeclarativeWindowManager(std::shared_ptr<mir::shell::WindowManager> const& default_wm,
        SurfaceGeometries const& positions_by_name, SurfaceDepths const& depths_by_name);

    virtual ~DeclarativeWindowManager() = default;
    
    mir::scene::SurfaceCreationParameters place(mir::scene::Session const& session, mir::scene::SurfaceCreationParameters const& request_parameters) override;

protected:
    DeclarativeWindowManager(const DeclarativeWindowManager&) = delete;
    DeclarativeWindowManager& operator=(const DeclarativeWindowManager&) = delete;

private:
    std::shared_ptr<mir::shell::WindowManager> const default_wm;

    SurfaceGeometries const& surface_geometries_by_name;
    SurfaceDepths const& surface_depths_by_name;
};
}

#endif // MIR_TEST_FRAMEWORK_DECLARATIVE_WINDOW_MANAGER_H_
