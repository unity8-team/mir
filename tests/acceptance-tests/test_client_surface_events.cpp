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
 * Authored by: Nick Dedekind <nick.dedekind@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/mir_client_library_debug.h"

#include "mir/shell/surface_coordinator_wrapper.h"

#include "mir_test_framework/stubbed_server_configuration.h"
#include "mir_test_framework/basic_client_server_fixture.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <condition_variable>
#include <chrono>
#include <mutex>

namespace mtf = mir_test_framework;
namespace ms = mir::scene;
namespace msh = mir::shell;

using namespace testing;

namespace
{
struct SceneSurface // TODO remove dummy
{
    void orientation(MirOrientation) {}
};


struct MockSurfaceCoordinator : msh::SurfaceCoordinatorWrapper
{
    using msh::SurfaceCoordinatorWrapper::SurfaceCoordinatorWrapper;

    // TODO need mocked functions to hook into server side
};

struct MyConfig : mtf::StubbedServerConfiguration
{
    std::shared_ptr<ms::SurfaceCoordinator> wrap_surface_coordinator(
        std::shared_ptr<ms::SurfaceCoordinator> const& wrapped) override
    {
        auto const msc = std::make_shared<MockSurfaceCoordinator>(wrapped);
        mock_surface_coordinator = msc;
        return msc;
    }

    std::shared_ptr<MockSurfaceCoordinator> the_mock_surface_coordinator() const
    {
        return mock_surface_coordinator.lock();
    }

    std::weak_ptr<MockSurfaceCoordinator> mock_surface_coordinator;
};

using BasicClientServerFixture = mtf::BasicClientServerFixture<MyConfig>;

struct ClientSurfaceEvents : BasicClientServerFixture
{
    MirSurfaceParameters const request_params
    {
        __FILE__,
        640, 480,
        mir_pixel_format_abgr_8888,
        mir_buffer_usage_hardware,
        mir_display_output_id_invalid
    };

    MirSurface* surface{nullptr};
    MirSurface* other_surface;

    MirEvent last_event{};
    MirSurface* last_event_surface = nullptr;
    MirEventDelegate delegate{&event_callback, this};

    std::shared_ptr<SceneSurface> scene_surface{new SceneSurface()};

    static void event_callback(MirSurface* surface, MirEvent const* event, void* ctx)
    {
        ClientSurfaceEvents* self = static_cast<ClientSurfaceEvents*>(ctx);
        self->last_event = *event;
        self->last_event_surface = surface;
    }

    bool receive_event_within(std::chrono::milliseconds) { return true; } // TODO

    void reset_last_event()
    {
        memset(&last_event, 0, sizeof last_event);
        last_event_surface = nullptr;
    }

    void SetUp() override
    {
        BasicClientServerFixture::SetUp();

        surface = mir_connection_create_surface_sync(connection, &request_params);
        mir_surface_set_event_handler(surface, &delegate);

        other_surface = mir_connection_create_surface_sync(connection, &request_params);
        mir_surface_set_event_handler(other_surface, nullptr);
    }

    void TearDown() override
    {
        mir_surface_release_sync(other_surface);
        mir_surface_release_sync(surface);

        BasicClientServerFixture::TearDown();
    }
};
}

TEST_F(ClientSurfaceEvents, surface_receives_state_events)
{
    int surface_id = mir_debug_surface_id(surface);

    mir_wait_for(mir_surface_set_state(surface, mir_surface_state_fullscreen));
    mir_wait_for(mir_surface_set_state(other_surface, mir_surface_state_minimized));
    EXPECT_THAT(last_event_surface, Eq(surface));
    EXPECT_THAT(last_event.type, Eq(mir_event_type_surface));
    EXPECT_THAT(last_event.surface.id, Eq(surface_id));
    EXPECT_THAT(last_event.surface.attrib, Eq(mir_surface_attrib_state));
    EXPECT_THAT(last_event.surface.value, Eq(mir_surface_state_fullscreen));

    mir_wait_for(mir_surface_set_state(surface, static_cast<MirSurfaceState>(999)));
    EXPECT_THAT(last_event_surface, Eq(surface));
    EXPECT_THAT(last_event.type, Eq(mir_event_type_surface));
    EXPECT_THAT(last_event.surface.id, Eq(surface_id));
    EXPECT_THAT(last_event.surface.attrib, Eq(mir_surface_attrib_state));
    EXPECT_THAT(last_event.surface.value, Eq(mir_surface_state_fullscreen));

    reset_last_event();

    mir_wait_for(mir_surface_set_state(surface, mir_surface_state_minimized));
    EXPECT_THAT(last_event_surface, Eq(surface));
    EXPECT_THAT(last_event.type, Eq(mir_event_type_surface));
    EXPECT_THAT(last_event.surface.id, Eq(surface_id));
    EXPECT_THAT(last_event.surface.attrib, Eq(mir_surface_attrib_state));
    EXPECT_THAT(last_event.surface.value, Eq(mir_surface_state_minimized));

    reset_last_event();

    mir_wait_for(mir_surface_set_state(surface, static_cast<MirSurfaceState>(777)));
    mir_wait_for(mir_surface_set_state(other_surface, mir_surface_state_maximized));
    EXPECT_THAT(last_event_surface, IsNull());
    EXPECT_THAT(last_event.type, Eq(0));
    EXPECT_THAT(last_event.surface.id, Eq(0));
    EXPECT_THAT(last_event.surface.attrib, Eq(0));
    EXPECT_THAT(last_event.surface.value, Eq(0));
}

struct OrientationEvents : ClientSurfaceEvents, ::testing::WithParamInterface<MirOrientation> {};

TEST_P(OrientationEvents, surface_receives_orientation_events)
{
    auto const direction = GetParam();

    scene_surface->orientation(direction);

    EXPECT_TRUE(receive_event_within(std::chrono::milliseconds(100)));

    EXPECT_THAT(last_event_surface, Eq(surface));
    EXPECT_THAT(last_event.type, Eq(mir_event_type_orientation));
    EXPECT_THAT(last_event.orientation.direction, Eq(direction));
}

INSTANTIATE_TEST_CASE_P(ClientSurfaceEvents,
    OrientationEvents,
    Values(mir_orientation_normal, mir_orientation_left, mir_orientation_inverted, mir_orientation_right));
