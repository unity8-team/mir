/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "playground/demo-shell/demo_compositor.h"

#include "mir/geometry/rectangle.h"
#include "mir/compositor/scene_element.h"
#include "mir/compositor/scene.h"
#include "src/server/scene/surface_stack.h"
#include "src/server/scene/basic_surface.h"
#include "src/server/report/null_report_factory.h"

#include "mir_test_doubles/mock_gl.h"
#include "mir_test_doubles/stub_display_buffer.h"
#include "mir_test_doubles/stub_buffer_stream.h"
#include "mir_test_doubles/stub_renderable.h"
#include "mir_test/fake_shared.h"
#include "mir_test/gmock_fixes.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;
namespace me = mir::examples;
namespace mt = mir::test;
namespace ms = mir::scene;
namespace mc = mir::compositor;

namespace
{

struct MockSceneElement : mc::SceneElement
{
    MOCK_CONST_METHOD0(renderable, std::shared_ptr<mir::graphics::Renderable>());
    MOCK_CONST_METHOD0(decoration, std::unique_ptr<mc::Decoration>());
    MOCK_METHOD0(rendered, void());
    MOCK_METHOD0(occluded, void());
};

struct DemoCompositor : testing::Test
{
    testing::NiceMock<mtd::MockGL> mock_gl;
    mtd::StubDisplayBuffer stub_display_buffer{
        geom::Rectangle{{0,0}, {1000,1000}}};
    ms::BasicSurface stub_surface{
        std::string("stub"),
        geom::Rectangle{{0,0},{100,100}},
        false,
        std::make_shared<mtd::StubBufferStream>(),
        std::shared_ptr<mir::input::InputChannel>(),
        std::shared_ptr<mir::input::InputSender>(),
        std::shared_ptr<mir::graphics::CursorImage>(),
        mir::report::null_scene_report()};

    testing::NiceMock<MockSceneElement> element;
    mc::SceneElementSequence scene_elements{mt::fake_shared(element)};

    void SetUp()
    {
        using namespace testing;
        ON_CALL(mock_gl, glCreateShader(_))
            .WillByDefault(Return(12));
        ON_CALL(mock_gl, glCreateProgram())
            .WillByDefault(Return(34));
        ON_CALL(mock_gl, glGetShaderiv(_, _, _))
            .WillByDefault(SetArgPointee<2>(GL_TRUE));
        ON_CALL(mock_gl, glGetProgramiv(_, _, _))
            .WillByDefault(SetArgPointee<2>(GL_TRUE));
    }
};

}

