/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#include "mir_toolkit/mir_client_library.h"

#include "mir/compositor/scene.h"
#include "mir/scene/null_observer.h"
#include "mir/scene/surface.h"
#include "mir/shell/surface_coordinator_wrapper.h"
#include "mir/shell/focus_setter.h"

#include "mir_test_framework/stubbed_server_configuration.h"
#include "mir_test_framework/basic_client_server_fixture.h"
#include "mir_test/event_matchers.h"
#include "mir_test/wait_condition.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mtf = mir_test_framework;
namespace mt = mir::test;
namespace ms = mir::scene;
namespace msh = mir::shell;

namespace
{

struct StoringSurfaceCoordinator : msh::SurfaceCoordinatorWrapper
{
    using msh::SurfaceCoordinatorWrapper::SurfaceCoordinatorWrapper;

    std::shared_ptr<ms::Surface> add_surface(
        ms::SurfaceCreationParameters const& params,
        ms::Session* session) override
    {
        auto const surface = msh::SurfaceCoordinatorWrapper::add_surface(params, session);
        surfaces.push_back(surface);
        return surface;
    }

    std::vector<std::weak_ptr<ms::Surface>> surfaces;

};

struct StubServerConfig : mtf::StubbedServerConfiguration
{
    std::shared_ptr<ms::SurfaceCoordinator> wrap_surface_coordinator(
        std::shared_ptr<ms::SurfaceCoordinator> const& wrapped) override
    {
        return std::make_shared<StoringSurfaceCoordinator>(wrapped);
    }

    std::shared_ptr<msh::FocusSetter> the_shell_focus_setter() override
    {
        struct NullFocusSetter : msh::FocusSetter
        {
            void set_focus_to(std::shared_ptr<ms::Session> const& ) override {}
        };
        return std::make_shared<NullFocusSetter>();
    }
};

using BasicFixture = mtf::BasicClientServerFixture<StubServerConfig>;

struct MockVisibilityCallback
{
    MOCK_METHOD2(handle, void(MirSurface*,MirSurfaceVisibility));
};

void event_callback(MirSurface* surface, MirEvent const* event, void* ctx)
{
    if (event->type == mir_event_type_surface &&
        event->surface.attrib == mir_surface_attrib_visibility)
    {
        auto const mock_visibility_callback =
            reinterpret_cast<testing::NiceMock<MockVisibilityCallback>*>(ctx);
        mock_visibility_callback->handle(
            surface,
            static_cast<MirSurfaceVisibility>(event->surface.value));
    }
}

struct MirSurfaceVisibilityTest : BasicFixture
{
    void SetUp()
    {
        BasicFixture::SetUp();

        client_create_surface();
    }

    void TearDown()
    {
        mir_surface_release_sync(surface);
        if (second_surface)
            mir_surface_release_sync(second_surface);

        BasicFixture::TearDown();
    }

    void client_create_surface()
    {
        MirSurfaceParameters const request_params =
        {
            __PRETTY_FUNCTION__,
            640, 480,
            mir_pixel_format_abgr_8888,
            mir_buffer_usage_hardware,
            mir_display_output_id_invalid
        };

        surface = mir_connection_create_surface_sync(connection, &request_params);
        ASSERT_TRUE(mir_surface_is_valid(surface));

        MirEventDelegate delegate{&event_callback, &mock_visibility_callback};
        mir_surface_set_event_handler(surface, &delegate);

        mir_surface_swap_buffers_sync(surface);
    }

    void create_larger_surface_on_top()
    {
        MirSurfaceParameters const request_params =
        {
            __PRETTY_FUNCTION__,
            800, 600,
            mir_pixel_format_bgr_888,
            mir_buffer_usage_hardware,
            mir_display_output_id_invalid
        };

        second_surface = mir_connection_create_surface_sync(connection, &request_params);
        ASSERT_TRUE(mir_surface_is_valid(second_surface));

        server_config().the_surface_coordinator()->raise(server_surface(1));

        mir_surface_swap_buffers_sync(second_surface);
    }

    std::shared_ptr<ms::Surface> server_surface(size_t index)
    {
        auto const storing_surface_coordinator =
            std::dynamic_pointer_cast<StoringSurfaceCoordinator>(server_config().the_surface_coordinator());

        return storing_surface_coordinator->surfaces[index].lock();
    }

    void move_surface_off_screen()
    {
        server_surface(0)->move_to(mir::geometry::Point{10000, 10000});
    }

    void move_surface_into_screen()
    {
        server_surface(0)->move_to(mir::geometry::Point{0, 0});
    }

    void raise_surface_on_top()
    {
        server_config().the_surface_coordinator()->raise(server_surface(0));
    }

    void expect_surface_visibility_event_after(
        std::function<void()> const& action,
        MirSurfaceVisibility visibility)
    {
        using namespace testing;

        mt::WaitCondition event_received;

        EXPECT_CALL(mock_visibility_callback, handle(surface, visibility))
            .WillOnce(mt::WakeUp(&event_received));

        action();

        event_received.wait_for_at_most_seconds(2);

        Mock::VerifyAndClearExpectations(&mock_visibility_callback);
    }

    MirSurface* surface = nullptr;
    MirSurface* second_surface = nullptr;
    testing::NiceMock<MockVisibilityCallback> mock_visibility_callback;
};

}

TEST_F(MirSurfaceVisibilityTest, client_gets_notification_when_surface_goes_off_screen)
{
    expect_surface_visibility_event_after(
        [this] { move_surface_off_screen(); },
        mir_surface_visibility_occluded);
}

TEST_F(MirSurfaceVisibilityTest, client_gets_notification_when_surface_reenters_screen)
{
    expect_surface_visibility_event_after(
        [this] { move_surface_off_screen(); },
        mir_surface_visibility_occluded);

    expect_surface_visibility_event_after(
        [this] { move_surface_into_screen(); },
        mir_surface_visibility_exposed);
}

TEST_F(MirSurfaceVisibilityTest, client_gets_notification_when_surface_occluded_by_other_surface)
{
    expect_surface_visibility_event_after(
        [this] { create_larger_surface_on_top(); },
        mir_surface_visibility_occluded);
}

TEST_F(MirSurfaceVisibilityTest, client_gets_notification_when_surface_raised_over_occluding_surface)
{
    expect_surface_visibility_event_after(
        [this] { create_larger_surface_on_top(); },
        mir_surface_visibility_occluded);

    expect_surface_visibility_event_after(
        [this] { raise_surface_on_top(); },
        mir_surface_visibility_exposed);
}
