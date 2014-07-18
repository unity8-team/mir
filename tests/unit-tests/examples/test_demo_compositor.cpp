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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "examples/demo-shell/demo_compositor.h"
#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/mock_display_buffer.h"
#include "mir_test_doubles/stub_gl_program_factory.h"
#include "mir_test_doubles/mock_scene.h"
#include "mir_test_doubles/stub_scene_element.h"
#include "mir_test_doubles/stub_renderable.h"
#include "mir_test_doubles/mock_gl.h"
#include "mir_test/fake_shared.h"
#include <gtest/gtest.h>

namespace me = mir::examples;
namespace mc = mir::compositor;
namespace mt = mir::test;
namespace mr = mir::report;
namespace mtd = mir::test::doubles;
namespace mg = mir::graphics;
namespace geom = mir::geometry;

struct DemoCompositor : public testing::Test
{
    DemoCompositor() :
        display_area{{10, 10}, {100,100}},
        titlebar_height{5},
        shadow_radius{3},
        stub_report{mr::null_compositor_report()},
        fullscreen{std::make_shared<mtd::StubSceneElement>(
            std::make_shared<mtd::StubRenderable>(display_area))},
        onscreen_titlebar{std::make_shared<mtd::StubSceneElement>(
            std::make_shared<mtd::StubRenderable>(geom::Rectangle{{90,90},{10,10}}))},
        onscreen_shadows{std::make_shared<mtd::StubSceneElement>(
            std::make_shared<mtd::StubRenderable>(geom::Rectangle{{10,10},{10,10}}))},
        onscreen_shadows_and_titlebar{std::make_shared<mtd::StubSceneElement>(
            std::make_shared<mtd::StubRenderable>(geom::Rectangle{{10,14},{10,82}}))}
    {
        using namespace testing;
        ON_CALL(mock_display_buffer, view_area())
            .WillByDefault(Return(display_area));
    }

    geom::Rectangle const display_area;
    geom::Height const titlebar_height;
    unsigned int const shadow_radius;
    std::shared_ptr<mc::CompositorReport> const stub_report;
    std::shared_ptr<mc::SceneElement> const fullscreen;
    std::shared_ptr<mc::SceneElement> const onscreen_titlebar;
    std::shared_ptr<mc::SceneElement> const onscreen_shadows;
    std::shared_ptr<mc::SceneElement> const onscreen_shadows_and_titlebar;

    testing::NiceMock<mtd::MockDisplayBuffer> mock_display_buffer;
    mtd::MockScene mock_scene;
    mtd::StubGLProgramFactory stub_program_factory;
    testing::NiceMock<mtd::MockGL> mock_gl;
};

TEST_F(DemoCompositor, does_not_use_optimized_path_if_titlebar_needs_to_be_drawn)
{
    using namespace testing;
    EXPECT_CALL(mock_scene, scene_elements_for(_))
        .WillOnce(Return(mc::SceneElementSequence{onscreen_titlebar}))
        .WillOnce(Return(mc::SceneElementSequence{onscreen_shadows}))
        .WillOnce(Return(mc::SceneElementSequence{onscreen_shadows_and_titlebar}));
    EXPECT_CALL(mock_display_buffer, post_renderables_if_optimizable(_))
        .Times(0);
    EXPECT_CALL(mock_display_buffer, post_update())
        .Times(3);

    me::DemoCompositor demo_compositor(
        mock_display_buffer, mt::fake_shared(mock_scene), stub_program_factory,
        stub_report, shadow_radius, titlebar_height);
    demo_compositor.composite();
    demo_compositor.composite();
    demo_compositor.composite();
}

TEST_F(DemoCompositor, uses_optimized_path_if_no_decorations_present)
{
    using namespace testing;
    EXPECT_CALL(mock_scene, scene_elements_for(_))
        .WillOnce(Return(mc::SceneElementSequence{fullscreen}))
        .WillOnce(Return(mc::SceneElementSequence{onscreen_shadows_and_titlebar, fullscreen}));
    EXPECT_CALL(mock_display_buffer, post_renderables_if_optimizable(_))
        .Times(2);

    me::DemoCompositor demo_compositor(
        mock_display_buffer, mt::fake_shared(mock_scene), stub_program_factory,
        stub_report, shadow_radius, titlebar_height);
    demo_compositor.composite();
    demo_compositor.composite();
}
