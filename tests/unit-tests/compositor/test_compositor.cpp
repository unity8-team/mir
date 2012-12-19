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

#include "mir/compositor/compositor.h"
#include "mir/surfaces/scenegraph.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/renderer.h"
#include "mir/graphics/display.h"
#include "mir/geometry/rectangle.h"
#include "mir_test_doubles/mock_display.h"
#include "mir_test_doubles/mock_renderable.h"
#include "mir_test_doubles/mock_surface_renderer.h"
#include "mir_test/empty_deleter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace ms = mir::surfaces;
namespace geom = mir::geometry;
namespace mg = mir::graphics;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

namespace
{

struct MockRenderView : mc::RenderView
{
    MOCK_METHOD2(for_each_if, void(mc::FilterForRenderables&, mc::OperatorForRenderables&));
};

struct FakeRenderView : mc::RenderView
{
    FakeRenderView(std::vector<mg::Renderable*> renderables) :
        renderables(renderables)
    {
    }

    // Ugly...should we use delegation?
    void for_each_if(mc::FilterForRenderables& filter, mc::OperatorForRenderables& renderable_operator)
    {
        for (auto it = renderables.begin(); it != renderables.end(); it++)
        {
            mg::Renderable &renderable = **it;
            if (filter(renderable)) renderable_operator(renderable);
        }
    }

    std::vector<mg::Renderable*> renderables;
};


}

TEST(Compositor, render)
{
    using namespace testing;

    mtd::MockSurfaceRenderer mock_renderer;
    std::shared_ptr<mg::Renderer> renderer(
        &mock_renderer,
        mir::EmptyDeleter());
    MockRenderView render_view;
    mtd::MockDisplay display;

    mc::Compositor comp(&render_view, renderer);

    EXPECT_CALL(mock_renderer, render(_)).Times(0);

    EXPECT_CALL(display, view_area())
            .Times(1)
            .WillRepeatedly(Return(geom::Rectangle()));

    EXPECT_CALL(render_view, for_each_if(_,_))
                .Times(1);

    EXPECT_CALL(display, post_update())
            .Times(1);

    comp.render(&display);
}

TEST(Compositor, skips_invisible_renderables)
{
    using namespace testing;

    mtd::MockSurfaceRenderer mock_renderer;
    std::shared_ptr<mg::Renderer> renderer(
        &mock_renderer,
        mir::EmptyDeleter());
    NiceMock<mtd::MockDisplay> display;

    EXPECT_CALL(display, view_area())
            .Times(1)
            .WillRepeatedly(Return(geom::Rectangle()));
    
    NiceMock<mtd::MockRenderable> mr1, mr2, mr3;
    
    EXPECT_CALL(mr1, hidden()).WillOnce(Return(false));
    EXPECT_CALL(mr2, hidden()).WillOnce(Return(true));
    EXPECT_CALL(mr3, hidden()).WillOnce(Return(false));

    std::vector<mg::Renderable*> renderables;
    renderables.push_back(&mr1);
    renderables.push_back(&mr2);
    renderables.push_back(&mr3);
    
    EXPECT_CALL(mock_renderer, render(Ref(mr1))).Times(1);
    EXPECT_CALL(mock_renderer, render(Ref(mr2))).Times(0);
    EXPECT_CALL(mock_renderer, render(Ref(mr3))).Times(1);
    
    FakeRenderView render_view(renderables);

    mc::Compositor comp(&render_view, renderer);

    comp.render(&display);
}
