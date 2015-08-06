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

#ifndef MIR_SHELL_SYSTEM_COMPOSITOR_WINDOW_MANAGER_H_
#define MIR_SHELL_SYSTEM_COMPOSITOR_WINDOW_MANAGER_H_

#include "mir/shell/window_manager.h"

namespace mir
{
namespace scene { class PlacementStrategy; class SessionCoordinator; }
namespace shell
{
class FocusController;
class DisplayLayout;

/** Minimal window management for system compositing.
 */
class SystemCompositorWindowManager : public WindowManager
{
public:
    SystemCompositorWindowManager(
        FocusController* focus_controller,
        std::shared_ptr<shell::DisplayLayout> const& display_layout,
        std::shared_ptr<scene::SessionCoordinator> const& session_coordinator);

/** @name Customization points
 * These are the likely events that a system compositor will care about
 *  @{ */
    /// Called when a session first connects (before any surfaces are ready)
    virtual void on_session_added(std::shared_ptr<scene::Session> const& session) const;

    /// Called when a session disconnects
    virtual void on_session_removed(std::shared_ptr<scene::Session> const& session) const;

    /// Called the first time each surface owned by the session posts its first buffer
    virtual void on_session_ready(std::shared_ptr<scene::Session> const& session) const;
/** @} */

protected:
    FocusController* const focus_controller;
    std::shared_ptr<DisplayLayout> const display_layout;
    std::shared_ptr<scene::SessionCoordinator> const session_coordinator;

private:
    void add_session(std::shared_ptr<scene::Session> const& session) override;

    void remove_session(std::shared_ptr<scene::Session> const& session) override;

    frontend::SurfaceId add_surface(
        std::shared_ptr<scene::Session> const& session,
        scene::SurfaceCreationParameters const& params,
        std::function<frontend::SurfaceId(std::shared_ptr<scene::Session> const& session, scene::SurfaceCreationParameters const& params)> const& build) override;

    void modify_surface(
        std::shared_ptr<scene::Session> const& session,
        std::shared_ptr<scene::Surface> const& surface,
        SurfaceSpecification const& modifications) override;

    void remove_surface(
        std::shared_ptr<scene::Session> const& session,
        std::weak_ptr<scene::Surface> const& surface) override;

    void add_display(geometry::Rectangle const& area) override;

    void remove_display(geometry::Rectangle const& area) override;

    bool handle_keyboard_event(MirKeyboardEvent const* event) override;

    bool handle_touch_event(MirTouchEvent const* event) override;

    bool handle_pointer_event(MirPointerEvent const* event) override;

    int set_surface_attribute(
        std::shared_ptr<scene::Session> const& session,
        std::shared_ptr<scene::Surface> const& surface,
        MirSurfaceAttrib attrib,
        int value) override;
};
}
}


#endif /* MIR_SHELL_SYSTEM_COMPOSITOR_WINDOW_MANAGER_H_ */
