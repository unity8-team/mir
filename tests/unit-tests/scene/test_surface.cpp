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

#include "src/server/scene/basic_surface.h"
#include "src/server/scene/legacy_surface_change_notification.h"
#include "src/server/report/null_report_factory.h"
#include "mir/frontend/event_sink.h"
#include "mir/scene/surface_creation_parameters.h"
#include "mir/scene/surface_configurator.h"
#include "mir/scene/surface_event_source.h"
#include "mir/input/input_channel.h"

#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_input_surface.h"
#include "mir_test_doubles/stub_buffer.h"
#include "mir_test_doubles/mock_input_sender.h"
#include "mir_test_doubles/stub_input_channel.h"
#include "mir_test_doubles/stub_input_sender.h"
#include "mir_test/fake_shared.h"
#include "mir_test/client_event_matchers.h"

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

    EXPECT_EQ(ms::a_surface(), params);
}

TEST(SurfaceCreationParametersTest, builder_mutators)
{
    using namespace geom;
    Size const size{1024, 768};
    mg::BufferUsage const usage{mg::BufferUsage::hardware};
    MirPixelFormat const format{mir_pixel_format_abgr_8888};
    std::string name{"surface"};

    auto params = ms::a_surface().of_name(name)
                                 .of_size(size)
                                 .of_buffer_usage(usage)
                                 .of_pixel_format(format);

    EXPECT_EQ(name, params.name);
    EXPECT_EQ(size, params.size);
    EXPECT_EQ(usage, params.buffer_usage);
    EXPECT_EQ(format, params.pixel_format);
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
class StubEventSink : public mir::frontend::EventSink
{
public:
    void handle_event(MirEvent const&) override {}
    void handle_lifecycle_event(MirLifecycleState) override {}
    void handle_display_config_change(mir::graphics::DisplayConfiguration const&) override {}
};

struct MockEventSink : StubEventSink
{
    MOCK_METHOD1(handle_event, void(MirEvent const&));
};

struct StubSurfaceConfigurator : ms::SurfaceConfigurator
{
    int select_attribute_value(ms::Surface const&, MirSurfaceAttrib, int) override { return 0; }

    void attribute_set(ms::Surface const&, MirSurfaceAttrib, int) override { }
};


struct SurfaceCreation : public ::testing::Test
{
    virtual void SetUp()
    {
        using namespace testing;

        notification_count = 0;
        change_notification = [this]()
        {
            notification_count++;
        };

        surface_name = "test_surfaceA";
        pf = mir_pixel_format_abgr_8888;
        size = geom::Size{43, 420};
        rect = geom::Rectangle{geom::Point{geom::X{0}, geom::Y{0}}, size};
        stride = geom::Stride{4 * size.width.as_uint32_t()};
        mock_buffer_stream = std::make_shared<testing::NiceMock<mtd::MockBufferStream>>();

        ON_CALL(*mock_buffer_stream, acquire_client_buffer(_))
            .WillByDefault(InvokeArgument<0>(&stub_buffer));
    }

    std::string surface_name;
    std::shared_ptr<testing::NiceMock<mtd::MockBufferStream>> mock_buffer_stream;
    MirPixelFormat pf;
    geom::Stride stride;
    geom::Size size;
    geom::Rectangle rect;
    std::shared_ptr<ms::SceneReport> const report = mr::null_scene_report();
    std::function<void()> change_notification;
    int notification_count;
    mtd::StubBuffer stub_buffer;
    std::shared_ptr<mtd::StubInputSender> const stub_input_sender = std::make_shared<mtd::StubInputSender>();
    std::shared_ptr<StubSurfaceConfigurator> const stub_configurator = std::make_shared<StubSurfaceConfigurator>();
};

}

TEST_F(SurfaceCreation, test_surface_queries_stream_for_pf)
{
    using namespace testing;
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_CALL(*mock_buffer_stream, get_stream_pixel_format())
        .Times(1)
        .WillOnce(Return(pf));

    auto ret_pf = surf.pixel_format();

    EXPECT_EQ(ret_pf, pf);
}

TEST_F(SurfaceCreation, test_surface_gets_right_name)
{
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_EQ(surface_name, surf.name());
}

TEST_F(SurfaceCreation, test_surface_queries_state_for_size)
{
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_EQ(size, surf.size());
}

TEST_F(SurfaceCreation, test_surface_next_buffer)
{
    using namespace testing;
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    mtd::StubBuffer graphics_resource;

    EXPECT_CALL(*mock_buffer_stream, acquire_client_buffer(_))
        .Times(1)
        .WillOnce(InvokeArgument<0>(&graphics_resource));

    surf.swap_buffers(
        nullptr,
        [&graphics_resource](mg::Buffer* result){ EXPECT_THAT(result, Eq(&graphics_resource)); });
}

TEST_F(SurfaceCreation, test_surface_gets_ipc_from_stream)
{
    using namespace testing;

    mtd::StubBuffer stub_buffer;

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_CALL(*mock_buffer_stream, acquire_client_buffer(_))
        .Times(1)
        .WillOnce(InvokeArgument<0>(&stub_buffer));

    surf.swap_buffers(
        nullptr,
        [&stub_buffer](mg::Buffer* result){ EXPECT_THAT(result, Eq(&stub_buffer)); });
}

TEST_F(SurfaceCreation, test_surface_gets_top_left)
{
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    auto ret_top_left = surf.top_left();
    EXPECT_EQ(geom::Point(), ret_top_left);
}

TEST_F(SurfaceCreation, test_surface_move_to)
{
    geom::Point p{55, 66};

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    surf.move_to(p);
    EXPECT_EQ(p, surf.top_left());
}

TEST_F(SurfaceCreation, resize_updates_stream_and_state)
{
    using namespace testing;
    geom::Size const new_size{123, 456};

    EXPECT_CALL(*mock_buffer_stream, resize(new_size))
        .Times(1);

    auto const mock_event_sink = std::make_shared<MockEventSink>();
    auto const observer = std::make_shared<ms::SurfaceEventSource>(mf::SurfaceId(), mock_event_sink);

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    surf.add_observer(observer);

    ASSERT_THAT(surf.size(), Ne(new_size));

    EXPECT_CALL(*mock_event_sink, handle_event(_)).Times(1);
    surf.resize(new_size);
    EXPECT_THAT(surf.size(), Eq(new_size));
}

TEST_F(SurfaceCreation, duplicate_resize_ignored)
{
    using namespace testing;
    geom::Size const new_size{123, 456};
    auto const mock_event_sink = std::make_shared<MockEventSink>();
    auto const observer = std::make_shared<ms::SurfaceEventSource>(mf::SurfaceId(), mock_event_sink);

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    surf.add_observer(observer);

    ASSERT_THAT(surf.size(), Ne(new_size));

    EXPECT_CALL(*mock_buffer_stream, resize(new_size)).Times(1);
    EXPECT_CALL(*mock_event_sink, handle_event(_)).Times(1);
    surf.resize(new_size);
    EXPECT_THAT(surf.size(), Eq(new_size));

    Mock::VerifyAndClearExpectations(mock_buffer_stream.get());
    Mock::VerifyAndClearExpectations(mock_event_sink.get());

    EXPECT_CALL(*mock_buffer_stream, resize(_)).Times(0);
    EXPECT_CALL(*mock_event_sink, handle_event(_)).Times(0);
    surf.resize(new_size);
    EXPECT_THAT(surf.size(), Eq(new_size));
}

TEST_F(SurfaceCreation, unsuccessful_resize_does_not_update_state)
{
    using namespace testing;
    geom::Size const new_size{123, 456};

    EXPECT_CALL(*mock_buffer_stream, resize(new_size))
        .Times(1)
        .WillOnce(Throw(std::runtime_error("bad resize")));

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_THROW({
        surf.resize(new_size);
    }, std::runtime_error);

    EXPECT_EQ(size, surf.size());
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

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    for (auto &size : bad_sizes)
    {
        geom::Size expect_size = size;
        if (expect_size.width <= geom::Width{0})
            expect_size.width = geom::Width{1};
        if (expect_size.height <= geom::Height{0})
            expect_size.height = geom::Height{1};

        EXPECT_CALL(*mock_buffer_stream, resize(expect_size)).Times(1);
        EXPECT_NO_THROW({ surf.resize(size); });
        EXPECT_EQ(expect_size, surf.size());
    }
}

TEST_F(SurfaceCreation, test_get_input_channel)
{
    auto mock_channel = std::make_shared<MockInputChannel>();
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        mock_channel,
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_EQ(mock_channel, surf.input_channel());
}

TEST_F(SurfaceCreation, test_surface_set_alpha)
{
    using namespace testing;

    float alpha = 0.5f;
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    surf.set_alpha(alpha);
    EXPECT_FLOAT_EQ(alpha, surf.compositor_snapshot(nullptr)->alpha());
}

TEST_F(SurfaceCreation, test_surface_force_requests_to_complete)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_stream, force_requests_to_complete()).Times(Exactly(1));

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    surf.force_requests_to_complete();
}

TEST_F(SurfaceCreation, test_surface_allow_framedropping)
{
    using namespace testing;

    EXPECT_CALL(*mock_buffer_stream, allow_framedropping(true))
        .Times(1);

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    surf.allow_framedropping(true);
}

TEST_F(SurfaceCreation, test_surface_next_buffer_tells_state_on_first_frame)
{
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    auto const observer = std::make_shared<ms::LegacySurfaceChangeNotification>(
        change_notification,
        [this](int){change_notification();});
    surf.add_observer(observer);

    mg::Buffer* buffer{nullptr};

    auto const complete = [&buffer](mg::Buffer* new_buffer){ buffer = new_buffer; };
    surf.swap_buffers(buffer, complete);
    surf.swap_buffers(buffer, complete);
    surf.swap_buffers(buffer, complete);
    surf.swap_buffers(buffer, complete);

    EXPECT_EQ(3, notification_count);
}

TEST_F(SurfaceCreation, input_fds)
{
    using namespace testing;

    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        std::shared_ptr<mi::InputChannel>(),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_THROW({
            surf.client_input_fd();
    }, std::logic_error);

    MockInputChannel channel;
    int const client_fd = 13;
    EXPECT_CALL(channel, client_fd()).Times(1).WillOnce(Return(client_fd));

    ms::BasicSurface input_surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        mt::fake_shared(channel),
        stub_input_sender,
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    EXPECT_EQ(client_fd, input_surf.client_input_fd());
}

TEST_F(SurfaceCreation, consume_calls_send_event)
{
    using namespace testing;

    NiceMock<mtd::MockInputSender> mock_sender;

    std::shared_ptr<mi::InputChannel> stub_channel = std::make_shared<mtd::StubInputChannel>();
    ms::BasicSurface surf(
        surface_name,
        rect,
        false,
        mock_buffer_stream,
        stub_channel,
        mt::fake_shared(mock_sender),
        stub_configurator,
        std::shared_ptr<mg::CursorImage>(),
        report);

    MirEvent key_event;
    MirEvent motion_event;
    std::memset(&key_event, 0, sizeof(key_event));
    std::memset(&motion_event, 0, sizeof(motion_event));
    key_event.type = mir_event_type_key;
    motion_event.type = mir_event_type_motion;

    EXPECT_CALL(mock_sender, send_event(mt::MirKeyEventMatches(key_event), stub_channel)).Times(1);
    EXPECT_CALL(mock_sender, send_event(mt::MirMotionEventMatches(motion_event), stub_channel)).Times(1);

    surf.consume(key_event);
    surf.consume(motion_event);
}