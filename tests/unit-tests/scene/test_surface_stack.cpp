/*
 * Copyright © 2012-2014 Canonical Ltd.
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "src/server/scene/surface_stack.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/geometry/rectangle.h"
#include "mir/scene/observer.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir/compositor/scene_element.h"
#include "src/server/report/null_report_factory.h"
#include "src/server/scene/basic_surface.h"
#include "mir/input/input_channel_factory.h"
#include "mir_test_doubles/stub_input_channel.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/stub_buffer_stream.h"
#include "mir_test_doubles/stub_renderable.h"
#include "mir_test_doubles/mock_buffer_stream.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <future>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mi = mir::input;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace mr = mir::report;

namespace
{

void post_a_frame(ms::Surface& s)
{
    mtd::StubBuffer old_buffer;
    s.swap_buffers(&old_buffer, [](mg::Buffer*){});
}

MATCHER_P(SurfaceWithInputReceptionMode, mode, "")
{
    return arg->reception_mode() == mode;
}

MATCHER_P(SceneElementFor, surface, "")
{
    return arg->renderable()->id() == surface.get();
}

struct StubInputChannelFactory : public mi::InputChannelFactory
{
    std::shared_ptr<mi::InputChannel> make_input_channel()
    {
        return std::make_shared<mtd::StubInputChannel>();
    }
};

struct StubInputChannel : public mi::InputChannel
{
    StubInputChannel(int server_fd, int client_fd)
        : s_fd(server_fd),
          c_fd(client_fd)
    {
    }

    int client_fd() const
    {
        return c_fd;
    }
    int server_fd() const
    {
        return s_fd;
    }

    int const s_fd;
    int const c_fd;
};

struct MockCallback
{
    MOCK_METHOD0(call, void());
};

struct MockSceneObserver : public ms::Observer
{
    MOCK_METHOD1(surface_added, void(ms::Surface*));
    MOCK_METHOD1(surface_removed, void(ms::Surface*));
    MOCK_METHOD0(surfaces_reordered, void());
    MOCK_METHOD0(scene_changed, void());

    MOCK_METHOD1(surface_exists, void(ms::Surface*));
    MOCK_METHOD0(end_observation, void());
};

struct SurfaceStack : public ::testing::Test
{
    void SetUp()
    {
        using namespace testing;
        default_params = ms::a_surface().of_size(geom::Size{geom::Width{1024}, geom::Height{768}});

        stub_surface1 = std::make_shared<ms::BasicSurface>(
            std::string("stub"),
            geom::Rectangle{{},{}},
            false,
            std::make_shared<mtd::StubBufferStream>(),
            std::shared_ptr<mir::input::InputChannel>(),
            std::shared_ptr<mir::input::InputSender>(),
            std::shared_ptr<mg::CursorImage>(),
            report);

        post_a_frame(*stub_surface1);

        stub_surface2 = std::make_shared<ms::BasicSurface>(
            std::string("stub"),
            geom::Rectangle{{},{}},
            false,
            std::make_shared<mtd::StubBufferStream>(),
            std::shared_ptr<mir::input::InputChannel>(),
            std::shared_ptr<mir::input::InputSender>(),
            std::shared_ptr<mg::CursorImage>(),
            report);

        post_a_frame(*stub_surface2);

        stub_surface3 = std::make_shared<ms::BasicSurface>(
            std::string("stub"),
            geom::Rectangle{{},{}},
            false,
            std::make_shared<mtd::StubBufferStream>(),
            std::shared_ptr<mir::input::InputChannel>(),
            std::shared_ptr<mir::input::InputSender>(),
            std::shared_ptr<mg::CursorImage>(),
            report);

        post_a_frame(*stub_surface3);

        invisible_stub_surface = std::make_shared<ms::BasicSurface>(
            std::string("stub"),
            geom::Rectangle{{},{}},
            false,
            std::make_shared<mtd::StubBufferStream>(),
            std::shared_ptr<mir::input::InputChannel>(),
            std::shared_ptr<mir::input::InputSender>(),
            std::shared_ptr<mg::CursorImage>(),
            report);
    }

    ms::SurfaceCreationParameters default_params;
    std::shared_ptr<ms::BasicSurface> stub_surface1;
    std::shared_ptr<ms::BasicSurface> stub_surface2;
    std::shared_ptr<ms::BasicSurface> stub_surface3;
    std::shared_ptr<ms::BasicSurface> invisible_stub_surface;

    std::shared_ptr<ms::SceneReport> const report = mr::null_scene_report();
    ms::SurfaceStack stack{report};
    void const* compositor_id{&default_params};
};

}

TEST_F(SurfaceStack, owns_surface_from_add_to_remove)
{
    using namespace testing;

    auto const use_count = stub_surface1.use_count();

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);

    EXPECT_THAT(stub_surface1.use_count(), Gt(use_count));

    stack.remove_surface(stub_surface1);

    EXPECT_THAT(stub_surface1.use_count(), Eq(use_count));
}

TEST_F(SurfaceStack, stacking_order)
{
    using namespace testing;

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface3, default_params.depth, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2),
            SceneElementFor(stub_surface3)));
}

TEST_F(SurfaceStack, scene_snapshot_omits_invisible_surfaces)
{
    using namespace testing;

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(invisible_stub_surface, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2)));
}

TEST_F(SurfaceStack, scene_counts_pending_accurately)
{
    using namespace testing;

    ms::SurfaceStack stack{report};
    auto surface = std::make_shared<ms::BasicSurface>(
        std::string("stub"),
        geom::Rectangle{{},{}},
        false,
        std::make_shared<mtd::StubBufferStream>(),
        std::shared_ptr<mir::input::InputChannel>(),
        std::shared_ptr<mir::input::InputSender>(),
        std::shared_ptr<mg::CursorImage>(),
        report);
    stack.add_surface(surface, default_params.depth, default_params.input_mode);

    EXPECT_EQ(0, stack.frames_pending(this));
    post_a_frame(*surface);
    post_a_frame(*surface);
    post_a_frame(*surface);
    EXPECT_EQ(3, stack.frames_pending(this));

    for (int expect = 3; expect >= 0; --expect)
    {
        ASSERT_EQ(expect, stack.frames_pending(this));
        auto snap = stack.scene_elements_for(compositor_id);
        for (auto& element : snap)
        {
            auto consumed = element->renderable()->buffer();
        }
    }
}

TEST_F(SurfaceStack, surfaces_are_emitted_by_layer)
{
    using namespace testing;

    stack.add_surface(stub_surface1, ms::DepthId{0}, default_params.input_mode);
    stack.add_surface(stub_surface2, ms::DepthId{1}, default_params.input_mode);
    stack.add_surface(stub_surface3, ms::DepthId{0}, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface3),
            SceneElementFor(stub_surface2)));
}


TEST_F(SurfaceStack, input_registrar_is_notified_of_input_monitor_scene)
{
    using namespace ::testing;

    MockSceneObserver observer;

    stack.add_observer(mt::fake_shared(observer));


    Sequence seq;
    EXPECT_CALL(observer, surface_added(SurfaceWithInputReceptionMode(mi::InputReceptionMode::receives_all_input)))
        .InSequence(seq);
    EXPECT_CALL(observer, surface_removed(_))
        .InSequence(seq);

    stack.add_surface(stub_surface1, default_params.depth, mi::InputReceptionMode::receives_all_input);
    stack.remove_surface(stub_surface1);
}

TEST_F(SurfaceStack, raise_to_top_alters_render_ordering)
{
    using namespace ::testing;

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface3, default_params.depth, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2),
            SceneElementFor(stub_surface3)));

    stack.raise(stub_surface1);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface2),
            SceneElementFor(stub_surface3),
            SceneElementFor(stub_surface1)));
}

TEST_F(SurfaceStack, depth_id_trumps_raise)
{
    using namespace ::testing;

    stack.add_surface(stub_surface1, ms::DepthId{0}, default_params.input_mode);
    stack.add_surface(stub_surface2, ms::DepthId{0}, default_params.input_mode);
    stack.add_surface(stub_surface3, ms::DepthId{1}, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2),
            SceneElementFor(stub_surface3)));

    stack.raise(stub_surface1);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface2),
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface3)));
}

TEST_F(SurfaceStack, raise_throw_behavior)
{
    using namespace ::testing;

    std::shared_ptr<ms::BasicSurface> null_surface{nullptr};
    EXPECT_THROW({
            stack.raise(null_surface);
    }, std::runtime_error);
}

TEST_F(SurfaceStack, generate_elementelements)
{
    using namespace testing;

    size_t num_surfaces{3};

    std::vector<std::shared_ptr<ms::Surface>> surfaces;
    for(auto i = 0u; i < num_surfaces; i++)
    {
        auto const surface = std::make_shared<ms::BasicSurface>(
            std::string("stub"),
            geom::Rectangle{geom::Point{3 * i, 4 * i},geom::Size{1 * i, 2 * i}},
            true,
            std::make_shared<mtd::StubBufferStream>(),
            std::shared_ptr<mir::input::InputChannel>(),
            std::shared_ptr<mir::input::InputSender>(),
            std::shared_ptr<mg::CursorImage>(),
            report);
        post_a_frame(*surface);

        surfaces.emplace_back(surface);
        stack.add_surface(surface, default_params.depth, default_params.input_mode);
    }

    auto const elements = stack.scene_elements_for(compositor_id);

    ASSERT_THAT(elements.size(), Eq(num_surfaces));

    auto surface_it = surfaces.begin();
    for(auto& element : elements)
    {
        EXPECT_THAT(element->renderable()->screen_position().top_left, Eq((*surface_it++)->top_left()));
    }

    for(auto& surface : surfaces)
        stack.remove_surface(surface);
}

TEST_F(SurfaceStack, scene_observer_notified_of_add_and_remove)
{
    using namespace ::testing;

    MockSceneObserver observer;

    InSequence seq;
    EXPECT_CALL(observer, surface_added(stub_surface1.get())).Times(1);
    EXPECT_CALL(observer, surface_removed(stub_surface1.get()))
        .Times(1);

    stack.add_observer(mt::fake_shared(observer));

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.remove_surface(stub_surface1);
}

TEST_F(SurfaceStack, multiple_observers)
{
    using namespace ::testing;

    MockSceneObserver observer1, observer2;

    InSequence seq;
    EXPECT_CALL(observer1, surface_added(stub_surface1.get())).Times(1);
    EXPECT_CALL(observer2, surface_added(stub_surface1.get())).Times(1);

    stack.add_observer(mt::fake_shared(observer1));
    stack.add_observer(mt::fake_shared(observer2));

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
}

TEST_F(SurfaceStack, remove_scene_observer)
{
    using namespace ::testing;

    MockSceneObserver observer;

    InSequence seq;
    EXPECT_CALL(observer, surface_added(stub_surface1.get())).Times(1);
    // We remove the scene observer before removing the surface, and thus
    // expect to NOT see the surface_removed call
    EXPECT_CALL(observer, end_observation()).Times(1);
    EXPECT_CALL(observer, surface_removed(stub_surface1.get()))
        .Times(0);

    stack.add_observer(mt::fake_shared(observer));

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.remove_observer(mt::fake_shared(observer));

    stack.remove_surface(stub_surface1);
}

// Many clients of the scene observer wish to install surface observers to monitor surface
// notifications. We offer them a surface_added event for existing surfaces to give them
// a chance to do this.
TEST_F(SurfaceStack, scene_observer_informed_of_existing_surfaces)
{
    using namespace ::testing;

    using namespace ::testing;

    MockSceneObserver observer;

    InSequence seq;
    EXPECT_CALL(observer, surface_exists(stub_surface1.get())).Times(1);
    EXPECT_CALL(observer, surface_exists(stub_surface2.get())).Times(1);
    
    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);

    stack.add_observer(mt::fake_shared(observer));
}

TEST_F(SurfaceStack, surfaces_reordered)
{
    using namespace ::testing;

    MockSceneObserver observer;

    EXPECT_CALL(observer, surface_added(_)).Times(AnyNumber());
    EXPECT_CALL(observer, surface_removed(_)).Times(AnyNumber());

    EXPECT_CALL(observer, surfaces_reordered()).Times(1);

    stack.add_observer(mt::fake_shared(observer));

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);
    stack.raise(stub_surface1);
}

TEST_F(SurfaceStack, scene_elements_hold_snapshot_of_positioning_info)
{
    size_t num_surfaces{3};

    std::vector<std::shared_ptr<ms::Surface>> surfaces;
    for(auto i = 0u; i < num_surfaces; i++)
    {
        auto const surface = std::make_shared<ms::BasicSurface>(
            std::string("stub"),
            geom::Rectangle{geom::Point{3 * i, 4 * i},geom::Size{1 * i, 2 * i}},
            true,
            std::make_shared<mtd::StubBufferStream>(),
            std::shared_ptr<mir::input::InputChannel>(),
            std::shared_ptr<mir::input::InputSender>(),
            std::shared_ptr<mg::CursorImage>(),
            report);

        surfaces.emplace_back(surface);
        stack.add_surface(surface, default_params.depth, default_params.input_mode);
    }

    auto const elements = stack.scene_elements_for(compositor_id);

    auto const changed_position = geom::Point{43,44};
    for(auto const& surface : surfaces)
        surface->move_to(changed_position);

    //check that the renderables are not at changed_pos
    for(auto& element : elements)
        EXPECT_THAT(changed_position, testing::Ne(element->renderable()->screen_position().top_left));
}

TEST_F(SurfaceStack, generates_scene_elements_that_delay_buffer_acquisition)
{
    using namespace testing;

    auto mock_stream = std::make_shared<NiceMock<mtd::MockBufferStream>>();
    EXPECT_CALL(*mock_stream, lock_compositor_buffer(_))
        .Times(0);

    auto const surface = std::make_shared<ms::BasicSurface>(
        std::string("stub"),
        geom::Rectangle{geom::Point{3, 4},geom::Size{1, 2}},
        true,
        mock_stream,
        std::shared_ptr<mir::input::InputChannel>(),
        std::shared_ptr<mir::input::InputSender>(),
        std::shared_ptr<mg::CursorImage>(),
        report);
    post_a_frame(*surface);
    stack.add_surface(surface, default_params.depth, default_params.input_mode);

    auto const elements = stack.scene_elements_for(compositor_id);

    Mock::VerifyAndClearExpectations(mock_stream.get());
    EXPECT_CALL(*mock_stream, lock_compositor_buffer(compositor_id))
        .Times(1)
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));
    ASSERT_THAT(elements.size(), Eq(1u));
    elements.front()->renderable()->buffer();
}

TEST_F(SurfaceStack, generates_scene_elements_that_allow_only_one_buffer_acquisition)
{
    using namespace testing;

    auto mock_stream = std::make_shared<NiceMock<mtd::MockBufferStream>>();
    EXPECT_CALL(*mock_stream, lock_compositor_buffer(_))
        .Times(1)
        .WillOnce(Return(std::make_shared<mtd::StubBuffer>()));

    auto const surface = std::make_shared<ms::BasicSurface>(
        std::string("stub"),
        geom::Rectangle{geom::Point{3, 4},geom::Size{1, 2}},
        true,
        mock_stream,
        std::shared_ptr<mir::input::InputChannel>(),
        std::shared_ptr<mir::input::InputSender>(),
        std::shared_ptr<mg::CursorImage>(),
        report);
    post_a_frame(*surface);
    stack.add_surface(surface, default_params.depth, default_params.input_mode);

    auto const elements = stack.scene_elements_for(compositor_id);
    ASSERT_THAT(elements.size(), Eq(1u));
    elements.front()->renderable()->buffer();
    elements.front()->renderable()->buffer();
    elements.front()->renderable()->buffer();
}

namespace
{
struct MockConfigureSurface : public ms::BasicSurface
{
    MockConfigureSurface() :
        ms::BasicSurface(
            {},
            {{},{}},
            true,
            std::make_shared<mtd::StubBufferStream>(),
            {},
            {},
            {},
            mir::report::null_scene_report())
    {
    }
    MOCK_METHOD2(configure, int(MirSurfaceAttrib, int));
};
}

TEST_F(SurfaceStack, occludes_not_rendered_surface)
{
    using namespace testing;

    mc::CompositorID const compositor_id2{&compositor_id};

    stack.register_compositor(compositor_id);
    stack.register_compositor(compositor_id2);

    auto const mock_surface = std::make_shared<MockConfigureSurface>();
    mock_surface->show();
    post_a_frame(*mock_surface);

    stack.add_surface(mock_surface, default_params.depth, default_params.input_mode);

    auto const elements = stack.scene_elements_for(compositor_id);
    ASSERT_THAT(elements.size(), Eq(1u));
    auto const elements2 = stack.scene_elements_for(compositor_id2);
    ASSERT_THAT(elements2.size(), Eq(1u));

    EXPECT_CALL(*mock_surface, configure(mir_surface_attrib_visibility, mir_surface_visibility_occluded));

    elements.back()->occluded();
    elements2.back()->occluded();
}

TEST_F(SurfaceStack, exposes_rendered_surface)
{
    using namespace testing;

    mc::CompositorID const compositor_id2{&compositor_id};

    stack.register_compositor(compositor_id);
    stack.register_compositor(compositor_id2);

    auto const mock_surface = std::make_shared<MockConfigureSurface>();
    post_a_frame(*mock_surface);
    stack.add_surface(mock_surface, default_params.depth, default_params.input_mode);

    auto const elements = stack.scene_elements_for(compositor_id);
    ASSERT_THAT(elements.size(), Eq(1u));
    auto const elements2 = stack.scene_elements_for(compositor_id2);
    ASSERT_THAT(elements2.size(), Eq(1u));

    EXPECT_CALL(*mock_surface, configure(mir_surface_attrib_visibility, mir_surface_visibility_exposed));

    elements.back()->occluded();
    elements2.back()->rendered();
}

TEST_F(SurfaceStack, occludes_surface_when_unregistering_all_compositors_that_rendered_it)
{
    using namespace testing;

    mc::CompositorID const compositor_id2{&compositor_id};
    mc::CompositorID const compositor_id3{&compositor_id2};

    stack.register_compositor(compositor_id);
    stack.register_compositor(compositor_id2);
    stack.register_compositor(compositor_id3);

    auto const mock_surface = std::make_shared<MockConfigureSurface>();
    post_a_frame(*mock_surface);
    stack.add_surface(mock_surface, default_params.depth, default_params.input_mode);

    auto const elements = stack.scene_elements_for(compositor_id);
    ASSERT_THAT(elements.size(), Eq(1u));
    auto const elements2 = stack.scene_elements_for(compositor_id2);
    ASSERT_THAT(elements2.size(), Eq(1u));
    auto const elements3 = stack.scene_elements_for(compositor_id3);
    ASSERT_THAT(elements3.size(), Eq(1u));

    EXPECT_CALL(*mock_surface, configure(mir_surface_attrib_visibility, mir_surface_visibility_exposed))
        .Times(2);

    elements.back()->occluded();
    elements2.back()->rendered();
    elements3.back()->rendered();

    Mock::VerifyAndClearExpectations(mock_surface.get());

    EXPECT_CALL(*mock_surface, configure(mir_surface_attrib_visibility, mir_surface_visibility_occluded));

    stack.unregister_compositor(compositor_id2);
    stack.unregister_compositor(compositor_id3);
}

TEST_F(SurfaceStack, observer_can_trigger_state_change_within_notification)
{
    using namespace ::testing;

    MockSceneObserver observer;

    auto const state_changer = [&]{
        stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    };

    //Make sure another thread can also change state
    auto const async_state_changer = [&]{
        std::async(std::launch::async, state_changer);
    };

    EXPECT_CALL(observer, surface_added(stub_surface1.get())).Times(3)
        .WillOnce(InvokeWithoutArgs(state_changer))
        .WillOnce(InvokeWithoutArgs(async_state_changer))
        .WillOnce(Return());

    stack.add_observer(mt::fake_shared(observer));

    state_changer();
}

TEST_F(SurfaceStack, observer_can_remove_itself_within_notification)
{
    using namespace testing;

    MockSceneObserver observer1;
    MockSceneObserver observer2;
    MockSceneObserver observer3;

    auto const remove_observer = [&]{
        stack.remove_observer(mt::fake_shared(observer2));
    };

    //Both of these observers should still get their notifications
    //regardless of the removal of observer2
    EXPECT_CALL(observer1, surface_added(stub_surface1.get())).Times(2);
    EXPECT_CALL(observer3, surface_added(stub_surface1.get())).Times(2);

    InSequence seq;
    EXPECT_CALL(observer2, surface_added(stub_surface1.get())).Times(1)
         .WillOnce(InvokeWithoutArgs(remove_observer));
    EXPECT_CALL(observer2, end_observation()).Times(1);

    stack.add_observer(mt::fake_shared(observer1));
    stack.add_observer(mt::fake_shared(observer2));
    stack.add_observer(mt::fake_shared(observer3));

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
}

TEST_F(SurfaceStack, scene_observer_notified_of_add_and_remove_input_visualization)
{
    using namespace ::testing;

    MockSceneObserver observer;
    mtd::StubRenderable r;

    InSequence seq;
    EXPECT_CALL(observer, scene_changed()).Times(2);

    stack.add_observer(mt::fake_shared(observer));

    stack.add_input_visualization(mt::fake_shared(r));
    stack.remove_input_visualization(mt::fake_shared(r));
}

TEST_F(SurfaceStack, overlays_do_not_appear_in_input_enumeration)
{
    mtd::StubRenderable r;
    
    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);

    // Configure surface1 and surface2 to appear in input enumeration.
    stub_surface1->configure(mir_surface_attrib_visibility, MirSurfaceVisibility::mir_surface_visibility_exposed);
    stub_surface2->configure(mir_surface_attrib_visibility, MirSurfaceVisibility::mir_surface_visibility_exposed);

    stack.add_input_visualization(mt::fake_shared(r));

    unsigned int observed_input_targets = 0;
    stack.for_each([&observed_input_targets](std::shared_ptr<mi::Surface> const&)
        {
            observed_input_targets++;
        });
    EXPECT_EQ(2, observed_input_targets);
}

TEST_F(SurfaceStack, overlays_appear_at_top_of_renderlist)
{
    using namespace ::testing;

    mtd::StubRenderable r;
    
    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_input_visualization(mt::fake_shared(r));
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2),
            SceneElementFor(mt::fake_shared(r))));    
}

TEST_F(SurfaceStack, removed_overlays_are_removed)
{
    using namespace ::testing;

    mtd::StubRenderable r;
    
    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_input_visualization(mt::fake_shared(r));
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2),
            SceneElementFor(mt::fake_shared(r))));
    
    stack.remove_input_visualization(mt::fake_shared(r));

    EXPECT_THAT(
        stack.scene_elements_for(compositor_id),
        ElementsAre(
            SceneElementFor(stub_surface1),
            SceneElementFor(stub_surface2)));
}

TEST_F(SurfaceStack, scene_observers_notified_of_generic_scene_change)
{
    MockSceneObserver o1, o2;

    EXPECT_CALL(o1, scene_changed()).Times(1);
    EXPECT_CALL(o2, scene_changed()).Times(1);
    
    stack.add_observer(mt::fake_shared(o1));
    stack.add_observer(mt::fake_shared(o2));
    
    stack.emit_scene_changed();
}

TEST_F(SurfaceStack, only_enumerates_exposed_input_surfaces)
{
    using namespace ::testing;

    stack.add_surface(stub_surface1, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface2, default_params.depth, default_params.input_mode);
    stack.add_surface(stub_surface3, default_params.depth, default_params.input_mode);

    stub_surface1->configure(mir_surface_attrib_visibility, MirSurfaceVisibility::mir_surface_visibility_exposed);
    stub_surface2->configure(mir_surface_attrib_visibility, MirSurfaceVisibility::mir_surface_visibility_occluded);
    stub_surface3->configure(mir_surface_attrib_visibility, MirSurfaceVisibility::mir_surface_visibility_occluded);

    int num_exposed_surfaces = 0;
    auto const count_exposed_surfaces = [&num_exposed_surfaces](std::shared_ptr<mi::Surface> const&){
        num_exposed_surfaces++;
    };

    stack.for_each(count_exposed_surfaces);
    EXPECT_THAT(num_exposed_surfaces, Eq(1));
}
