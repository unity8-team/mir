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

#include "mir/events/event_private.h"
#include "mir/events/event_builders.h"
#include "src/server/scene/basic_surface.h"
#include "src/server/scene/legacy_surface_change_notification.h"
#include "src/server/report/null_report_factory.h"
#include "mir/frontend/event_sink.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir/scene/surface_event_source.h"
#include "mir/input/input_channel.h"

#include "mir/test/doubles/mock_buffer_stream.h"
#include "mir/test/doubles/mock_input_surface.h"
#include "mir/test/doubles/stub_buffer.h"
#include "mir/test/doubles/mock_input_sender.h"
#include "mir/test/doubles/stub_input_channel.h"
#include "mir/test/doubles/stub_input_sender.h"
#include "mir/test/doubles/null_event_sink.h"
#include "mir/test/fake_shared.h"
#include "mir/test/event_matchers.h"

#include "mir/cookie_factory.h"

#include "gmock_set_arg.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdexcept>
#include <cstring>

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace mev = mir::events;
namespace geom = mir::geometry;
namespace mt = mir::test;
namespace mtd = mt::doubles;
namespace mr = mir::report;

namespace
{
struct MockInputChannel : public mi::InputChannel
{
    MOCK_CONST_METHOD0(server_fd, int());
    MOCK_CONST_METHOD0(client_fd, int());
};
}

TEST(SurfaceCreationParametersTest, default_creation_parameters)
{
    using namespace geom;
    ms::SurfaceCreationParameters params;

    geom::Point const default_point{geom::X{0}, geom::Y{0}};

    EXPECT_EQ(std::string(), params.name);
    EXPECT_EQ(Width(0), params.size.width);
    EXPECT_EQ(Height(0), params.size.height);
    EXPECT_EQ(default_point, params.top_left);
    EXPECT_EQ(mg::BufferUsage::undefined, params.buffer_usage);
    EXPECT_EQ(mir_pixel_format_invalid, params.pixel_format);
    EXPECT_FALSE(params.type.is_set());
    EXPECT_FALSE(params.state.is_set());
    EXPECT_FALSE(params.preferred_orientation.is_set());
    EXPECT_FALSE(params.parent_id.is_set());

    EXPECT_EQ(ms::a_surface(), params);
}

TEST(SurfaceCreationParametersTest, builder_mutators)
{
    using namespace geom;
    Size const size{1024, 768};
    mg::BufferUsage const usage{mg::BufferUsage::hardware};
    MirPixelFormat const format{mir_pixel_format_abgr_8888};
    std::string name{"surface"};
    MirSurfaceState state{mir_surface_state_fullscreen};
    MirSurfaceType type{mir_surface_type_dialog};
    MirOrientationMode mode{mir_orientation_mode_landscape};
    mf::SurfaceId surf_id{1000};

    auto params = ms::a_surface()
        .of_name(name)
        .of_size(size)
        .of_buffer_usage(usage)
        .of_pixel_format(format)
        .of_type(type)
        .with_parent_id(surf_id)
        .with_preferred_orientation(mode)
        .with_state(state);

    EXPECT_EQ(name, params.name);
    EXPECT_EQ(size, params.size);
    EXPECT_EQ(usage, params.buffer_usage);
    EXPECT_EQ(format, params.pixel_format);

    EXPECT_EQ(type, params.type);
    EXPECT_EQ(state, params.state);
    EXPECT_EQ(mode, params.preferred_orientation);
    EXPECT_EQ(surf_id, params.parent_id);
}

TEST(SurfaceCreationParametersTest, equality)
{
    using namespace geom;
    Size const size{1024, 768};
    mg::BufferUsage const usage{mg::BufferUsage::hardware};
    MirPixelFormat const format{mir_pixel_format_abgr_8888};

    auto params0 = ms::a_surface().of_name("surface")
                                  .of_size(size)
                                  .of_buffer_usage(usage)
                                  .of_pixel_format(format);

    auto params1 = ms::a_surface().of_name("surface")
                                  .of_size(size)
                                  .of_buffer_usage(usage)
                                  .of_pixel_format(format);

    EXPECT_EQ(params0, params1);
    EXPECT_EQ(params1, params0);
}

TEST(SurfaceCreationParametersTest, inequality)
{
    using namespace geom;

    std::vector<Size> const sizes{{1024, 768},
                                  {1025, 768}};

    std::vector<mg::BufferUsage> const usages{mg::BufferUsage::hardware,
                                              mg::BufferUsage::software};

    std::vector<MirPixelFormat> const formats{mir_pixel_format_abgr_8888,
                                                 mir_pixel_format_bgr_888};

    std::vector<ms::SurfaceCreationParameters> params_vec;

    for (auto const& size : sizes)
    {
        for (auto const& usage : usages)
        {
            for (auto const& format : formats)
            {
                auto cur_params = ms::a_surface().of_name("surface0")
                                                 .of_size(size)
                                                 .of_buffer_usage(usage)
                                                 .of_pixel_format(format);
                params_vec.push_back(cur_params);
                size_t cur_index = params_vec.size() - 1;

                /*
                 * Compare the current SurfaceCreationParameters with all the previously
                 * created ones.
                 */
                for (size_t i = 0; i < cur_index; i++)
                {
                    EXPECT_NE(params_vec[i], params_vec[cur_index]) << "cur_index: " << cur_index << " i: " << i;
                    EXPECT_NE(params_vec[cur_index], params_vec[i]) << "cur_index: " << cur_index << " i: " << i;
                }

            }
        }
    }
}

namespace
{
struct MockEventSink : mtd::NullEventSink
{
    MOCK_METHOD1(handle_event, void(MirEvent const&));
};

struct SurfaceCreation : public ::testing::Test
{
    SurfaceCreation()
        : surface(surface_name,
            rect, false, mock_buffer_stream, 
            std::make_shared<mtd::StubInputChannel>(),
            std::make_shared<mtd::StubInputSender>(),
            nullptr /* cursor_image */, report,
            mt::fake_shared(cookie_factory))
    {
    }

    virtual void SetUp()
    {
        using namespace testing;

        notification_count = 0;
        change_notification = [this]()
        {
            notification_count++;
        };

        ON_CALL(*mock_buffer_stream, acquire_client_buffer(_))
            .WillByDefault(InvokeArgument<0>(&stub_buffer));
    }

    std::shared_ptr<testing::NiceMock<mtd::MockBufferStream>> mock_buffer_stream = std::make_shared<testing::NiceMock<mtd::MockBufferStream>>();
    std::function<void()> change_notification;
    int notification_count = 0;
    mtd::StubBuffer stub_buffer;
    
    std::string surface_name = "test_surfaceA";
    MirPixelFormat pf = mir_pixel_format_abgr_8888;
    geom::Size size = geom::Size{43, 420};
    geom::Stride stride = geom::Stride{4 * size.width.as_uint32_t()};
    geom::Rectangle rect = geom::Rectangle{geom::Point{geom::X{0}, geom::Y{0}}, size};
    std::shared_ptr<ms::SceneReport> const report = mr::null_scene_report();
    std::vector<uint8_t> secret{ 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0xde, 0x01 };
    mir::CookieFactory cookie_factory{secret};
    ms::BasicSurface surface;
};

}

TEST_F(SurfaceCreation, test_surface_gets_right_name)
{
    EXPECT_EQ(surface_name, surface.name());
}

TEST_F(SurfaceCreation, test_surface_queries_state_for_size)
{
    EXPECT_EQ(size, surface.size());
}

TEST_F(SurfaceCreation, constructed_stream_is_primary)
{
    using namespace testing;
    EXPECT_THAT(surface.primary_buffer_stream(), Eq(mock_buffer_stream));
}

TEST_F(SurfaceCreation, test_surface_gets_top_left)
{
    auto ret_top_left = surface.top_left();
    EXPECT_EQ(geom::Point(), ret_top_left);
}

TEST_F(SurfaceCreation, test_surface_move_to)
{
    geom::Point p{55, 66};

    surface.move_to(p);
    EXPECT_EQ(p, surface.top_left());
}

TEST_F(SurfaceCreation, resize_updates_stream_and_state)
{
    using namespace testing;
    geom::Size const new_size{123, 456};

    EXPECT_CALL(*mock_buffer_stream, resize(new_size))
        .Times(1);

    auto const mock_event_sink = std::make_shared<MockEventSink>();
    auto const observer = std::make_shared<ms::SurfaceEventSource>(mf::SurfaceId(), mock_event_sink);

    surface.add_observer(observer);

    ASSERT_THAT(surface.size(), Ne(new_size));

    EXPECT_CALL(*mock_event_sink, handle_event(_)).Times(1);
    surface.resize(new_size);
    EXPECT_THAT(surface.size(), Eq(new_size));
}

TEST_F(SurfaceCreation, duplicate_resize_ignored)
{
    using namespace testing;
    geom::Size const new_size{123, 456};
    auto const mock_event_sink = std::make_shared<MockEventSink>();
    auto const observer = std::make_shared<ms::SurfaceEventSource>(mf::SurfaceId(), mock_event_sink);

    surface.add_observer(observer);

    ASSERT_THAT(surface.size(), Ne(new_size));

    EXPECT_CALL(*mock_buffer_stream, resize(new_size)).Times(1);
    EXPECT_CALL(*mock_event_sink, handle_event(_)).Times(1);
    surface.resize(new_size);
    EXPECT_THAT(surface.size(), Eq(new_size));

    Mock::VerifyAndClearExpectations(mock_buffer_stream.get());
    Mock::VerifyAndClearExpectations(mock_event_sink.get());

    EXPECT_CALL(*mock_buffer_stream, resize(_)).Times(0);
    EXPECT_CALL(*mock_event_sink, handle_event(_)).Times(0);
    surface.resize(new_size);
    EXPECT_THAT(surface.size(), Eq(new_size));
}

TEST_F(SurfaceCreation, unsuccessful_resize_does_not_update_state)
{
    using namespace testing;
    geom::Size const new_size{123, 456};

    EXPECT_CALL(*mock_buffer_stream, resize(new_size))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("bad resize")));

    EXPECT_THROW({
        surface.resize(new_size);
    }, std::runtime_error);

    EXPECT_EQ(size, surface.size());
}

TEST_F(SurfaceCreation, impossible_resize_clamps)
{
    using namespace testing;

    geom::Size const bad_sizes[] =
    {
        {0, 123},
        {456, 0},
        {-1, -1},
        {78, -10},
        {0, 0}
    };

    for (auto &size : bad_sizes)
    {
        geom::Size expect_size = size;
        if (expect_size.width <= geom::Width{0})
            expect_size.width = geom::Width{1};
        if (expect_size.height <= geom::Height{0})
            expect_size.height = geom::Height{1};

        EXPECT_CALL(*mock_buffer_stream, resize(expect_size)).Times(1);
        EXPECT_NO_THROW({ surface.resize(size); });
        EXPECT_EQ(expect_size, surface.size());
    }
}

TEST_F(SurfaceCreation, test_surface_set_alpha)
{
    using namespace testing;

    float alpha = 0.5f;

    surface.set_alpha(alpha);
    EXPECT_FLOAT_EQ(alpha, surface.alpha());
    auto renderables = surface.generate_renderables(nullptr);
    ASSERT_THAT(renderables.size(), Ge(1));
    EXPECT_FLOAT_EQ(alpha, renderables[0]->alpha());
    
    alpha = 0.1;

    surface.set_alpha(alpha);
    EXPECT_FLOAT_EQ(alpha, surface.alpha());
    renderables = surface.generate_renderables(nullptr);
    ASSERT_THAT(renderables.size(), Ge(1));
    EXPECT_FLOAT_EQ(alpha, renderables[0]->alpha());
}

TEST_F(SurfaceCreation, input_fds)
{
    using namespace testing;

    MockInputChannel channel;
    int const client_fd = 13;
    EXPECT_CALL(channel, client_fd()).Times(AnyNumber()).WillRepeatedly(Return(client_fd));

    ms::BasicSurface input_surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        mt::fake_shared(channel),
        std::make_shared<mtd::StubInputSender>(),
        std::shared_ptr<mg::CursorImage>(),
        report,
        mt::fake_shared(cookie_factory));

    EXPECT_EQ(client_fd, input_surf.client_input_fd());
}

TEST_F(SurfaceCreation, consume_calls_send_event)
{
    using namespace testing;

    NiceMock<mtd::MockInputSender> mock_sender;
    ms::BasicSurface surface(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::make_shared<mtd::StubInputChannel>(),
        mt::fake_shared(mock_sender),
        std::shared_ptr<mg::CursorImage>(),
        report,
        mt::fake_shared(cookie_factory));

    auto key_event = mev::make_event(MirInputDeviceId(0), std::chrono::nanoseconds(0), 0,
                                     mir_keyboard_action_down, 0, 0, mir_input_event_modifier_none);
    auto touch_event = mev::make_event(MirInputDeviceId(0), std::chrono::nanoseconds(0), 0,
                                       mir_input_event_modifier_none);
    mev::add_touch(*touch_event, 0, mir_touch_action_down, mir_touch_tooltype_finger, 0, 0,
        0, 0, 0, 0);

    EXPECT_CALL(mock_sender, send_event(mt::MirKeyEventMatches(*key_event), _)).Times(1);
    EXPECT_CALL(mock_sender, send_event(mt::MirTouchEventMatches(*touch_event), _)).Times(1);

    surface.consume(*key_event);
    surface.consume(*touch_event);
}
