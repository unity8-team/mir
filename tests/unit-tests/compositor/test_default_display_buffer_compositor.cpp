/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "src/server/compositor/default_display_buffer_compositor.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "src/server/report/null_report_factory.h"
#include "mir/compositor/scene.h"
#include "mir/compositor/renderer.h"
#include "mir/geometry/rectangle.h"
#include "mir_test_doubles/mock_renderer.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_display_buffer.h"
#include "mir_test_doubles/mock_renderable.h"
#include "mir_test_doubles/fake_renderable.h"
#include "mir_test_doubles/stub_display_buffer.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/mock_compositor_report.h"
#include "mir_test_doubles/mock_scene.h"
#include "mir_test_doubles/stub_scene.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace geom = mir::geometry;
namespace ms = mir::scene;
namespace mr = mir::report;

namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{

struct FakeScene : mtd::StubScene
{
    FakeScene(mg::RenderableList const& renderlist)
        : renderlist{renderlist}
    {
    }

    mg::RenderableList renderable_list_for(void const*) const override
    {
        return renderlist;
    }

    void change(mg::RenderableList const& new_renderlist)
    {
        renderlist = new_renderlist;
    }

    mg::RenderableList renderlist;
};

struct DefaultDisplayBufferCompositor : public testing::Test
{
    DefaultDisplayBufferCompositor()
     : small(std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{10, 20},{30, 40}})),
       big(std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{5, 10},{100, 200}})),
       fullscreen(std::make_shared<mtd::FakeRenderable>(screen))
    {
        using namespace testing;
        ON_CALL(display_buffer, orientation())
            .WillByDefault(Return(mir_orientation_normal));
        ON_CALL(display_buffer, view_area())
            .WillByDefault(Return(screen));
        ON_CALL(display_buffer, post_renderables_if_optimizable(_))
            .WillByDefault(Return(false));
    }

    testing::NiceMock<mtd::MockRenderer> mock_renderer;
    geom::Rectangle screen{{0, 0}, {1366, 768}};
    testing::NiceMock<mtd::MockDisplayBuffer> display_buffer;
    std::shared_ptr<mtd::FakeRenderable> small;
    std::shared_ptr<mtd::FakeRenderable> big;
    std::shared_ptr<mtd::FakeRenderable> fullscreen;
};
}

TEST_F(DefaultDisplayBufferCompositor, render)
{
    using namespace testing;
    mtd::MockScene scene;

    mg::RenderableList const empty;
    EXPECT_CALL(mock_renderer, render(empty))
        .Times(1);
    EXPECT_CALL(display_buffer, view_area())
        .Times(AtLeast(1));
    EXPECT_CALL(display_buffer, make_current())
        .Times(1);
    EXPECT_CALL(scene, renderable_list_for(_))
        .Times(1);
    EXPECT_CALL(display_buffer, post_update())
        .Times(1);

    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        mr::null_compositor_report());
    compositor.composite();
}

TEST_F(DefaultDisplayBufferCompositor, skips_scene_that_should_not_be_rendered)
{
    using namespace testing;
    
    mtd::StubDisplayBuffer display_buffer{geom::Rectangle{{0,0},{14,14}}};
    auto mock_renderable1 = std::make_shared<NiceMock<mtd::MockRenderable>>();
    auto mock_renderable2 = std::make_shared<NiceMock<mtd::MockRenderable>>();
    auto mock_renderable3 = std::make_shared<NiceMock<mtd::MockRenderable>>();

    glm::mat4 simple;
    EXPECT_CALL(*mock_renderable1, transformation())
        .WillOnce(Return(simple));
    EXPECT_CALL(*mock_renderable2, transformation())
        .WillOnce(Return(simple));
    EXPECT_CALL(*mock_renderable3, transformation())
        .WillOnce(Return(simple));

    EXPECT_CALL(*mock_renderable1, visible())
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mock_renderable2, visible())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_renderable3, visible())
        .WillRepeatedly(Return(true));

    EXPECT_CALL(*mock_renderable1, alpha())
        .WillOnce(Return(1.0f));
    EXPECT_CALL(*mock_renderable3, alpha())
        .WillOnce(Return(1.0f));

    EXPECT_CALL(*mock_renderable1, shaped())
        .WillOnce(Return(false));
    EXPECT_CALL(*mock_renderable3, shaped())
        .WillOnce(Return(false));

    EXPECT_CALL(*mock_renderable1, screen_position())
        .WillRepeatedly(Return(geom::Rectangle{{1,2}, {3,4}}));
    EXPECT_CALL(*mock_renderable2, screen_position())
        .WillRepeatedly(Return(geom::Rectangle{{1,2}, {3,4}}));
    EXPECT_CALL(*mock_renderable3, screen_position())
        .WillRepeatedly(Return(geom::Rectangle{{5,6}, {7,8}}));

    mg::RenderableList list{
        mock_renderable1,
        mock_renderable2,
        mock_renderable3
    };
    FakeScene scene(list);

    mg::RenderableList const visible{mock_renderable1, mock_renderable3};
    EXPECT_CALL(mock_renderer, render(visible))
        .Times(1);

    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        mr::null_compositor_report());
    compositor.composite();
}

TEST_F(DefaultDisplayBufferCompositor, optimization_skips_composition)
{
    using namespace testing;
    FakeScene scene({});
    auto report = std::make_shared<mtd::MockCompositorReport>();

    Sequence seq;
    EXPECT_CALL(*report, began_frame(_))
        .InSequence(seq);
    EXPECT_CALL(display_buffer, post_renderables_if_optimizable(_))
        .InSequence(seq)
        .WillOnce(Return(true));
    EXPECT_CALL(mock_renderer, suspend())
        .InSequence(seq);
    EXPECT_CALL(*report, finished_frame(true,_))
        .InSequence(seq);

    EXPECT_CALL(mock_renderer, begin())
        .Times(0);
    EXPECT_CALL(mock_renderer, render(_))
        .Times(0);
    EXPECT_CALL(mock_renderer, end())
        .Times(0);

    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        report);
    compositor.composite();
}

TEST_F(DefaultDisplayBufferCompositor, calls_renderer_in_sequence)
{
    using namespace testing;
    mg::RenderableList list{
        big,
        small
    };
    FakeScene scene(list);

    Sequence render_seq;
    EXPECT_CALL(mock_renderer, suspend())
        .Times(0);
    EXPECT_CALL(display_buffer, make_current())
        .InSequence(render_seq);
    EXPECT_CALL(display_buffer, orientation())
        .InSequence(render_seq)
        .WillOnce(Return(mir_orientation_normal));
    EXPECT_CALL(mock_renderer, set_rotation(_))
        .InSequence(render_seq);
    EXPECT_CALL(mock_renderer, begin())
        .InSequence(render_seq);
    EXPECT_CALL(mock_renderer, render(list))
        .InSequence(render_seq);
    EXPECT_CALL(display_buffer, post_update())
        .InSequence(render_seq);
    EXPECT_CALL(mock_renderer, end())
        .InSequence(render_seq);

    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        mr::null_compositor_report());
    compositor.composite();
}

TEST_F(DefaultDisplayBufferCompositor, optimization_toggles_seamlessly)
{
    using namespace testing;
    ON_CALL(display_buffer, view_area())
        .WillByDefault(Return(screen));
    ON_CALL(display_buffer, orientation())
        .WillByDefault(Return(mir_orientation_normal));

    Sequence seq;
    EXPECT_CALL(display_buffer, post_renderables_if_optimizable(_))
        .InSequence(seq)
        .WillOnce(Return(false));

    EXPECT_CALL(display_buffer, make_current())
        .InSequence(seq);
    EXPECT_CALL(display_buffer, orientation())
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, set_rotation(mir_orientation_normal))
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, begin())
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, render(_))
        .InSequence(seq);
    EXPECT_CALL(display_buffer, post_update())
        .InSequence(seq);

    EXPECT_CALL(display_buffer, post_renderables_if_optimizable(_))
        .InSequence(seq)
        .WillOnce(Return(true));
    //we should be testing that post_buffer is called, not just that
    //we check the bits on the compositor buffer
    EXPECT_CALL(display_buffer, post_renderables_if_optimizable(_))
        .InSequence(seq)
        .WillOnce(Return(false));
    EXPECT_CALL(display_buffer, make_current())
        .InSequence(seq);
    EXPECT_CALL(display_buffer, orientation())
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, set_rotation(mir_orientation_normal))
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, begin())
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, render(_))
        .InSequence(seq);
    EXPECT_CALL(display_buffer, post_update())
        .InSequence(seq);

    FakeScene scene({});
    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        mr::null_compositor_report());

    compositor.composite();
    compositor.composite();
    compositor.composite();

    fullscreen->set_buffer({});  // Avoid GMock complaining about false leaks
}

TEST_F(DefaultDisplayBufferCompositor, occluded_surfaces_are_not_rendered)
{
    using namespace testing;
    EXPECT_CALL(display_buffer, view_area())
        .WillRepeatedly(Return(screen));
    EXPECT_CALL(display_buffer, orientation())
        .WillOnce(Return(mir_orientation_normal));
    EXPECT_CALL(display_buffer, post_renderables_if_optimizable(_))
        .WillRepeatedly(Return(false));

    auto window0 = std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{99,99},{2,2}});
    auto window1 = std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{10,10},{20,20}});
    auto window2 = std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{0,0},{100,100}});
    auto window3 = std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{0,0},{100,100}});
    auto window4 = std::make_shared<mtd::FakeRenderable>(geom::Rectangle{{0,0},{500,500}}, 1.0f, true, false, true);

    mg::RenderableList list({
        window0, //not occluded
        window1, //occluded
        window2, //occluded
        window3, //not occluded
        window4  //invisible
    });
    FakeScene scene(list);

    mg::RenderableList const visible{window0, window3};

    Sequence seq;
    EXPECT_CALL(display_buffer, make_current())
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, render(visible))
        .InSequence(seq);
    EXPECT_CALL(display_buffer, post_update())
        .InSequence(seq);

    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        mr::null_compositor_report());
    compositor.composite();
}

TEST_F(DefaultDisplayBufferCompositor, renderer_ends_after_post_update)
{
    using namespace testing;
    auto mock_renderable1 = std::make_shared<NiceMock<mtd::MockRenderable>>();
    auto mock_renderable2 = std::make_shared<NiceMock<mtd::MockRenderable>>();
    auto buf1 = std::make_shared<mtd::StubBuffer>();
    auto buf2 = std::make_shared<mtd::StubBuffer>();
    ON_CALL(*mock_renderable1, buffer())
        .WillByDefault(Return(buf1));
    ON_CALL(*mock_renderable2, buffer())
        .WillByDefault(Return(buf2));
    ON_CALL(display_buffer, view_area())
        .WillByDefault(Return(geom::Rectangle{{0,0},{14,14}}));
    EXPECT_CALL(*mock_renderable1, screen_position())
        .WillRepeatedly(Return(geom::Rectangle{{1,2}, {3,4}}));
    EXPECT_CALL(*mock_renderable2, screen_position())
        .WillRepeatedly(Return(geom::Rectangle{{0,2}, {3,4}}));

    mg::RenderableList list{
        mock_renderable1,
        mock_renderable2
    };
    FakeScene scene(list);

    Sequence seq;
    EXPECT_CALL(display_buffer, post_update())
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, end())
        .InSequence(seq);

    mc::DefaultDisplayBufferCompositor compositor(
        display_buffer,
        mt::fake_shared(scene),
        mt::fake_shared(mock_renderer),
        mr::null_compositor_report());
    compositor.composite();
}
