/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/shell/skeleton_window_manager.h"
#include "mir/scene/surface.h"

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;


void msh::SkeletonWindowManager::add_session(std::shared_ptr<scene::Session> const& /*session*/)
{
}

void msh::SkeletonWindowManager::remove_session(std::shared_ptr<scene::Session> const& /*session*/)
{
}

auto msh::SkeletonWindowManager::add_surface(
    std::shared_ptr<scene::Session> const& session,
    scene::SurfaceCreationParameters const& params,
    std::function<frontend::SurfaceId(std::shared_ptr<scene::Session> const& session, scene::SurfaceCreationParameters const& params)> const& build)
-> frontend::SurfaceId
{
    return build(session, params);
}

void msh::SkeletonWindowManager::remove_surface(
    std::shared_ptr<scene::Session> const& /*session*/,
    std::weak_ptr<scene::Surface> const& /*surface*/)
{
}

void msh::SkeletonWindowManager::add_display(geometry::Rectangle const& /*area*/)
{
}

void msh::SkeletonWindowManager::remove_display(geometry::Rectangle const& /*area*/)
{
}

bool msh::SkeletonWindowManager::handle_key_event(MirKeyInputEvent const* /*event*/)
{
    return false;
}

bool msh::SkeletonWindowManager::handle_touch_event(MirTouchInputEvent const* /*event*/)
{
    return false;
}

bool msh::SkeletonWindowManager::handle_pointer_event(MirPointerInputEvent const* /*event*/)
{
    return false;
}

int msh::SkeletonWindowManager::set_surface_attribute(
    std::shared_ptr<scene::Session> const& /*session*/,
    std::shared_ptr<scene::Surface> const& surface,
    MirSurfaceAttrib attrib,
    int value)
{
    return surface->configure(attrib, value);
}
