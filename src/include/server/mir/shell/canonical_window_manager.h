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

#ifndef MIR_SHELL_CANONICAL_WINDOW_MANAGER_H_
#define MIR_SHELL_CANONICAL_WINDOW_MANAGER_H_

#include "basic_window_manager.h"

#include "mir/geometry/displacement.h"

namespace mir
{
namespace shell
{
class DisplayLayout;

struct CanonicalSessionInfo
{
    int surfaces{0};
};

struct CanonicalSurfaceInfo
{
    CanonicalSurfaceInfo(
        std::shared_ptr<scene::Session> const& session,
        std::shared_ptr<scene::Surface> const& surface,
        scene::SurfaceCreationParameters const& params);

    MirSurfaceType type;
    MirSurfaceState state;
    geometry::Rectangle restore_rect;
    std::weak_ptr<scene::Session> session;
    std::weak_ptr<scene::Surface> parent;
    std::vector<std::weak_ptr<scene::Surface>> children;
    optional_value<geometry::Width> min_width;
    optional_value<geometry::Height> min_height;
    optional_value<geometry::Width> max_width;
    optional_value<geometry::Height> max_height;
    mir::optional_value<geometry::DeltaX> width_inc;
    mir::optional_value<geometry::DeltaY> height_inc;
    mir::optional_value<SurfaceAspectRatio> min_aspect;
    mir::optional_value<SurfaceAspectRatio> max_aspect;
};

// standard window management algorithm:
//  o Switch apps: tap or click on the corresponding tile
//  o Move window: Alt-leftmousebutton drag
//  o Resize window: Alt-middle_button drag
//  o Maximize/restore current window (to display size): Alt-F11
//  o Maximize/restore current window (to display height): Shift-F11
//  o Maximize/restore current window (to display width): Ctrl-F11
//  o client requests to maximize, vertically maximize & restore
class CanonicalWindowManagerPolicy
{
public:
    using Tools = BasicWindowManagerTools<CanonicalSessionInfo, CanonicalSurfaceInfo>;
    using CanonicalSessionInfoMap = typename SessionTo<CanonicalSessionInfo>::type;
    using CanonicalSurfaceInfoMap = typename SurfaceTo<CanonicalSurfaceInfo>::type;

    explicit CanonicalWindowManagerPolicy(
        Tools* const tools,
        std::shared_ptr<shell::DisplayLayout> const& display_layout);

    void click(geometry::Point cursor);

    void handle_session_info_updated(CanonicalSessionInfoMap& session_info, geometry::Rectangles const& displays);

    void handle_displays_updated(CanonicalSessionInfoMap& session_info, geometry::Rectangles const& displays);

    void resize(geometry::Point cursor);

    auto handle_place_new_surface(
        std::shared_ptr<scene::Session> const& session,
        scene::SurfaceCreationParameters const& request_parameters)
    -> scene::SurfaceCreationParameters;

    void handle_new_surface(std::shared_ptr<scene::Session> const& session, std::shared_ptr<scene::Surface> const& surface);

    void handle_modify_surface(
        std::shared_ptr<scene::Session> const& session,
        std::shared_ptr<scene::Surface> const& surface,
        SurfaceSpecification const& modifications);

    void handle_delete_surface(std::shared_ptr<scene::Session> const& session, std::weak_ptr<scene::Surface> const& surface);

    int handle_set_state(std::shared_ptr<scene::Surface> const& surface, MirSurfaceState value);

    void drag(geometry::Point cursor);

    bool handle_keyboard_event(MirKeyboardEvent const* event);

    bool handle_touch_event(MirTouchEvent const* event);

    bool handle_pointer_event(MirPointerEvent const* event);

    std::vector<std::shared_ptr<scene::Surface>> generate_decorations_for(
        std::shared_ptr<scene::Session> const& session, std::shared_ptr<scene::Surface> const& surface);

private:
    static const int modifier_mask =
        mir_input_event_modifier_alt |
        mir_input_event_modifier_shift |
        mir_input_event_modifier_sym |
        mir_input_event_modifier_ctrl |
        mir_input_event_modifier_meta;

    void toggle(MirSurfaceState state);

    // "Mir and Unity: Surfaces, input, and displays (v0.3)" talks about active
    //  *window*,but Mir really only understands surfaces
    void select_active_surface(std::shared_ptr<scene::Surface> const& surface);
    auto active_surface() const -> std::shared_ptr<scene::Surface>;

    bool resize(std::shared_ptr<scene::Surface> const& surface, geometry::Point cursor, geometry::Point old_cursor, geometry::Rectangle bounds);
    bool drag(std::shared_ptr<scene::Surface> surface, geometry::Point to, geometry::Point from, geometry::Rectangle bounds);
    void move_tree(std::shared_ptr<scene::Surface> const& root, geometry::Displacement movement) const;
    void raise_tree(std::shared_ptr<scene::Surface> const& root) const;
    bool constrained_resize(
        std::shared_ptr<scene::Surface> const& surface,
        geometry::Point const& requested_pos,
        geometry::Size const& requested_size,
        const bool left_resize,
        const bool top_resize,
        geometry::Rectangle const& bounds);

    Tools* const tools;
    std::shared_ptr<DisplayLayout> const display_layout;

    geometry::Rectangle display_area;
    geometry::Point old_cursor{};
    std::weak_ptr<scene::Surface> active_surface_;
};

using CanonicalWindowManager = BasicWindowManager<CanonicalWindowManagerPolicy, CanonicalSessionInfo, CanonicalSurfaceInfo>;
}
}

#endif /* MIR_SHELL_CANONICAL_WINDOW_MANAGER_H_ */
