/*
 * Copyright © 2013 Canonical Ltd.
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

#include "software_cursor_overlay_renderer.h"

#include "gl_cursor_renderer.h"

#include "mir/compositor/compositor.h"
#include "mir/graphics/display_buffer.h"

namespace me = mir::examples;
namespace mc = mir::compositor;
namespace mg = mir::graphics;

me::SoftwareCursorOverlayRenderer::SoftwareCursorOverlayRenderer()
{
}

void me::SoftwareCursorOverlayRenderer::cursor_moved_to(float abs_x, float abs_y)
{
    cursor_x = abs_x;
    cursor_y = abs_y;

    auto strong_damage_handler = damage_handler.lock();
    if (strong_damage_handler)
        strong_damage_handler->request_redraw();
}

void me::SoftwareCursorOverlayRenderer::render(mg::DisplayBuffer& display_buffer)
{
    if (!cursor_renderer) // We need to setup the renderer in the correct  GL context, so we do this here
        cursor_renderer = std::make_shared<me::GLCursorRenderer>();
    cursor_renderer->render_cursor(display_buffer.view_area().size, cursor_x, cursor_y);
}

void me::SoftwareCursorOverlayRenderer::set_damage_handler(std::shared_ptr<mc::Compositor> const& handler)
{
    damage_handler = handler;
}
