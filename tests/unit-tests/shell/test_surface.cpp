/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir/shell/surface.h"
#include "surfaces/surface.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface_stack_model.h"
#include "mir/shell/surface_builder.h"
#include "mir/frontend/event_sink.h"
#include "mir/graphics/display_configuration.h"

#include "mir_test_doubles/stub_surface_controller.h"
#include "mir_test_doubles/mock_surface_controller.h"
#include "mir_test_doubles/stub_buffer_stream.h"
#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/mock_input_targeter.h"
#include "mir_test_doubles/stub_input_targeter.h"
#include "mir_test_doubles/null_event_sink.h"
#include "mir_test_doubles/mock_surface_state.h"
#include "mir_test_doubles/null_surface_configurator.h"
#include "mir_test_doubles/mock_surface_configurator.h"
#include "mir_test/fake_shared.h"
#include "mir_test/event_matchers.h"

#include <stdexcept>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mf = mir::frontend;
namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{

struct MockEventSink : public mf::EventSink
{
    ~MockEventSink() noexcept(true) {}
    MOCK_METHOD1(handle_event, void(MirEvent const&));
    MOCK_METHOD1(handle_lifecycle_event, void(MirLifecycleState));
    MOCK_METHOD1(handle_display_config_change, void(mg::DisplayConfiguration const&));
};

class StubSurfaceBuilder : public msh::SurfaceBuilder
{
public:
    StubSurfaceBuilder() :
        stub_buffer_stream_(std::make_shared<mtd::StubBufferStream>()),
        dummy_surface()
    {
    }

    std::weak_ptr<ms::Surface> create_surface(msh::Session*, msh::SurfaceCreationParameters const& )
    {
        auto state = std::make_shared<mtd::MockSurfaceState>();
        dummy_surface = std::make_shared<ms::Surface>(state, stub_buffer_stream_, std::shared_ptr<mi::InputChannel>());
        return dummy_surface;
    }

    void destroy_surface(std::weak_ptr<ms::Surface> const& )
    {
        reset_surface();
    }

    void reset_surface()
    {
        dummy_surface.reset();
    }

    std::shared_ptr<mtd::StubBufferStream> stub_buffer_stream()
    {
        return stub_buffer_stream_;
    }

private:
    std::shared_ptr<mtd::StubBufferStream> const stub_buffer_stream_;
    std::shared_ptr<ms::Surface> dummy_surface;
};

class MockSurfaceBuilder : public msh::SurfaceBuilder
{
public:
    MockSurfaceBuilder()
    {
        using namespace testing;
        ON_CALL(*this, create_surface(_, _)).
            WillByDefault(Invoke(&self, &StubSurfaceBuilder::create_surface));

        ON_CALL(*this, destroy_surface(_)).
            WillByDefault(Invoke(&self, &StubSurfaceBuilder::destroy_surface));
    }

    MOCK_METHOD2(create_surface, std::weak_ptr<ms::Surface> (msh::Session*, const msh::SurfaceCreationParameters&));

    MOCK_METHOD1(destroy_surface, void (std::weak_ptr<ms::Surface> const&));

private:
    StubSurfaceBuilder self;
};

typedef testing::NiceMock<mtd::MockBufferStream> StubBufferStream;


struct ShellSurface : testing::Test
{
    std::shared_ptr<StubBufferStream> const buffer_stream;
    StubSurfaceBuilder surface_builder;
    mtd::StubSurfaceController surface_controller;

    ShellSurface() :
        buffer_stream(std::make_shared<StubBufferStream>()),
        stub_sender(std::make_shared<mtd::NullEventSink>())
    {
        using namespace testing;

        ON_CALL(*buffer_stream, stream_size()).WillByDefault(Return(geom::Size()));
        ON_CALL(*buffer_stream, get_stream_pixel_format()).WillByDefault(Return(geom::PixelFormat::abgr_8888));
        ON_CALL(*buffer_stream, secure_client_buffer()).WillByDefault(Return(std::shared_ptr<mtd::StubBuffer>()));
    }
    mf::SurfaceId stub_id;
    std::shared_ptr<mf::EventSink> stub_sender;
};
}

TEST_F(ShellSurface, creation_and_destruction)
{
    using namespace testing;

    msh::SurfaceCreationParameters const params;
    MockSurfaceBuilder surface_builder;

    InSequence sequence;
    EXPECT_CALL(surface_builder, create_surface(nullptr, params)).Times(1);
    EXPECT_CALL(surface_builder, destroy_surface(_)).Times(1);

    msh::Surface test(
        nullptr,
        mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
        params, stub_id, stub_sender);
}

TEST_F(ShellSurface, creation_throws_means_no_destroy)
{
    using namespace testing;

    msh::SurfaceCreationParameters const params;
    MockSurfaceBuilder surface_builder;

    InSequence sequence;
    EXPECT_CALL(surface_builder, create_surface(nullptr, params)).Times(1)
        .WillOnce(Throw(std::runtime_error(__PRETTY_FUNCTION__)));
    EXPECT_CALL(surface_builder, destroy_surface(_)).Times(Exactly(0));

    EXPECT_THROW({
        msh::Surface test(
            nullptr,
            mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
            params, stub_id, stub_sender);
    }, std::runtime_error);
}

TEST_F(ShellSurface, destroy)
{
    using namespace testing;
    MockSurfaceBuilder surface_builder;

    InSequence sequence;
    EXPECT_CALL(surface_builder, create_surface(nullptr, _)).Times(AnyNumber());
    EXPECT_CALL(surface_builder, destroy_surface(_)).Times(0);

    {
        msh::Surface test(
                          nullptr,
                          mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
                          msh::a_surface(), stub_id, stub_sender);
        
        Mock::VerifyAndClearExpectations(&test);
        EXPECT_CALL(surface_builder, destroy_surface(_)).Times(1);
        Mock::VerifyAndClearExpectations(&test);
    }

    EXPECT_CALL(surface_builder, destroy_surface(_)).Times(0);
}

TEST_F(ShellSurface, attributes)
{
    using namespace testing;

    msh::Surface surf(
            nullptr,
            mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
            msh::a_surface(), stub_id, stub_sender);

    EXPECT_THROW({
        surf.configure(static_cast<MirSurfaceAttrib>(111), 222);
    }, std::logic_error);
}

TEST_F(ShellSurface, types)
{
    using namespace testing;

    msh::Surface surf(
            nullptr,
            mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
            msh::a_surface(), stub_id, stub_sender);

    EXPECT_EQ(mir_surface_type_normal, surf.type());

    EXPECT_EQ(mir_surface_type_utility,
              surf.configure(mir_surface_attrib_type,
                             mir_surface_type_utility));
    EXPECT_EQ(mir_surface_type_utility, surf.type());

    EXPECT_THROW({
        surf.configure(mir_surface_attrib_type, 999);
    }, std::logic_error);
    EXPECT_THROW({
        surf.configure(mir_surface_attrib_type, -1);
    }, std::logic_error);
    EXPECT_EQ(mir_surface_type_utility, surf.type());

    EXPECT_EQ(mir_surface_type_dialog,
              surf.configure(mir_surface_attrib_type,
                             mir_surface_type_dialog));
    EXPECT_EQ(mir_surface_type_dialog, surf.type());

    EXPECT_EQ(mir_surface_type_freestyle,
              surf.configure(mir_surface_attrib_type,
                             mir_surface_type_freestyle));
    EXPECT_EQ(mir_surface_type_freestyle, surf.type());
}

TEST_F(ShellSurface, states)
{
    using namespace testing;

    msh::Surface surf(
            nullptr,
        mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
        msh::a_surface(), stub_id, stub_sender);

    EXPECT_EQ(mir_surface_state_restored, surf.state());

    EXPECT_EQ(mir_surface_state_vertmaximized,
              surf.configure(mir_surface_attrib_state,
                             mir_surface_state_vertmaximized));
    EXPECT_EQ(mir_surface_state_vertmaximized, surf.state());

    EXPECT_THROW({
        surf.configure(mir_surface_attrib_state, 999);
    }, std::logic_error);
    EXPECT_THROW({
        surf.configure(mir_surface_attrib_state, -1);
    }, std::logic_error);
    EXPECT_EQ(mir_surface_state_vertmaximized, surf.state());

    EXPECT_EQ(mir_surface_state_minimized,
              surf.configure(mir_surface_attrib_state,
                             mir_surface_state_minimized));
    EXPECT_EQ(mir_surface_state_minimized, surf.state());

    EXPECT_EQ(mir_surface_state_fullscreen,
              surf.configure(mir_surface_attrib_state,
                             mir_surface_state_fullscreen));
    EXPECT_EQ(mir_surface_state_fullscreen, surf.state());
}

TEST_F(ShellSurface, sends_focus_notifications_when_focus_gained_and_lost)
{
    using namespace testing;

    MockEventSink sink;
    
    {
        InSequence seq;
        EXPECT_CALL(sink, handle_event(mt::SurfaceEvent(mir_surface_attrib_focus, mir_surface_focused)))
            .Times(1);
        EXPECT_CALL(sink, handle_event(mt::SurfaceEvent(mir_surface_attrib_focus, mir_surface_unfocused)))
            .Times(1);
    }

    msh::Surface surf(
            nullptr,
            mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
            msh::a_surface(), stub_id, mt::fake_shared(sink));

    surf.configure(mir_surface_attrib_focus, mir_surface_focused);
    surf.configure(mir_surface_attrib_focus, mir_surface_unfocused);
}

TEST_F(ShellSurface, configurator_selects_attribute_values)
{
    using namespace testing;
    
    mtd::MockSurfaceConfigurator configurator;
    
    EXPECT_CALL(configurator, select_attribute_value(_, mir_surface_attrib_state, mir_surface_state_restored)).Times(1)
        .WillOnce(Return(mir_surface_state_minimized));
    EXPECT_CALL(configurator, attribute_set(_, mir_surface_attrib_state, mir_surface_state_minimized)).Times(1);

    msh::Surface surf(
            nullptr,
            mt::fake_shared(surface_builder), mt::fake_shared(configurator),
            msh::a_surface(), stub_id, stub_sender);

    EXPECT_EQ(mir_surface_state_minimized, surf.configure(mir_surface_attrib_state, mir_surface_state_restored));
}

TEST_F(ShellSurface, take_input_focus)
{
    using namespace ::testing;

    msh::Surface test(
        nullptr,
        mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
        msh::a_surface(), stub_id, stub_sender);
    
    mtd::MockInputTargeter targeter;
    EXPECT_CALL(targeter, focus_changed(_)).Times(1);
    
    test.take_input_focus(mt::fake_shared(targeter));
}

TEST_F(ShellSurface, with_most_recent_buffer_do_uses_compositor_buffer)
{
    msh::Surface test(
        nullptr,
        mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
        msh::a_surface(), stub_id, stub_sender);

    mg::Buffer* buf_ptr{nullptr};

    test.with_most_recent_buffer_do(
        [&](mg::Buffer& buffer)
        {
            buf_ptr = &buffer;
        });

    EXPECT_EQ(surface_builder.stub_buffer_stream()->stub_compositor_buffer.get(),
              buf_ptr);
}

TEST_F(ShellSurface, raise)
{
    using namespace ::testing;

    mtd::MockSurfaceController surface_controller;
    msh::Surface test(
        nullptr,
        mt::fake_shared(surface_builder), std::make_shared<mtd::NullSurfaceConfigurator>(),
        msh::a_surface(), stub_id, stub_sender);
    
    EXPECT_CALL(surface_controller, raise(_)).Times(1);

    test.raise(mt::fake_shared(surface_controller));
}
