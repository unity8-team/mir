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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "src/server/compositor/rendering_operator.h"
#include "mir/geometry/rectangle.h"
#include "mir_test_doubles/mock_renderer.h"
#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_renderable.h"
#include "mir_test_doubles/stub_buffer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;
namespace mg = mir::graphics;

TEST(RenderingOperator, render_operator_saves_resources)
{
    using namespace testing;

    unsigned long frameno = 84;
    mtd::MockRenderer mock_renderer;
    mtd::MockRenderable mock_renderable;
    auto stub_buffer0 = std::make_shared<mtd::StubBuffer>();
    auto stub_buffer1 = std::make_shared<mtd::StubBuffer>();
    auto stub_buffer2 = std::make_shared<mtd::StubBuffer>();

    EXPECT_CALL(mock_renderable, buffer(frameno))
        .Times(3)
        .WillOnce(Return(stub_buffer0))
        .WillOnce(Return(stub_buffer1))
        .WillOnce(Return(stub_buffer2));

    Sequence seq;
    EXPECT_CALL(mock_renderer, render(Ref(mock_renderable), Ref(*stub_buffer0)))
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, render(Ref(mock_renderable), Ref(*stub_buffer1)))
        .InSequence(seq);
    EXPECT_CALL(mock_renderer, render(Ref(mock_renderable), Ref(*stub_buffer2)))
        .InSequence(seq);

    auto use_count_before0 = stub_buffer0.use_count(); 
    auto use_count_before1 = stub_buffer1.use_count(); 
    auto use_count_before2 = stub_buffer2.use_count(); 
    {
        mc::RenderingOperator rendering_operator(mock_renderer, frameno);
        rendering_operator(mock_renderable);
        rendering_operator(mock_renderable);
        rendering_operator(mock_renderable);

        EXPECT_EQ(use_count_before0 + 1, stub_buffer0.use_count());
        EXPECT_EQ(use_count_before1 + 1, stub_buffer1.use_count());
        EXPECT_EQ(use_count_before2 + 1, stub_buffer2.use_count());
    }

    EXPECT_EQ(use_count_before0, stub_buffer0.use_count());
    EXPECT_EQ(use_count_before1, stub_buffer1.use_count());
    EXPECT_EQ(use_count_before2, stub_buffer2.use_count());
}
