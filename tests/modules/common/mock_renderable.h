/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MOCK_MIR_GRAPHICS_RENDERABLE_H
#define MOCK_MIR_GRAPHICS_RENDERABLE_H

#include <mir/graphics/renderable.h>
#include <gmock/gmock.h>

namespace testing
{
struct MockRenderable : public mir::graphics::Renderable
{
    MockRenderable() {};

    MOCK_CONST_METHOD0(id, ID());
    MOCK_CONST_METHOD0(buffer, std::shared_ptr<mir::graphics::Buffer>());
    MOCK_CONST_METHOD0(alpha_enabled, bool());
    MOCK_CONST_METHOD0(screen_position, mir::geometry::Rectangle());
    MOCK_CONST_METHOD0(alpha, float() );
    MOCK_CONST_METHOD0(transformation, glm::mat4());
    MOCK_CONST_METHOD0(visible, bool());
    MOCK_CONST_METHOD0(shaped, bool());
    MOCK_CONST_METHOD0(buffers_ready_for_compositor, int());
};
}

#endif // MOCK_MIR_GRAPHICS_RENDERABLE_H

