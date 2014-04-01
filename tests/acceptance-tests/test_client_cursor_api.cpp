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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "mir/graphics/cursor.h"
#include "mir/graphics/cursor_image.h"
#include "mir/graphics/cursor_images.h"

#include "mir_toolkit/mir_client_library.h"

#include "mir_test/fake_event_hub.h"
#include "mir_test/fake_shared.h"
#include "mir_test/event_factory.h"
#include "mir_test/wait_condition.h"
#include "mir_test_framework/display_server_test_fixture.h"
#include "mir_test_framework/input_testing_server_configuration.h"
#include "mir_test_framework/input_testing_client_configuration.h"
#include "mir_test_framework/declarative_placement_strategy.h"
#include "mir_test_framework/cross_process_sync.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mis = mir::input::synthesis;
namespace geom = mir::geometry;

namespace mt = mir::test;
namespace mtf = mir_test_framework;

namespace
{
    char const* const mir_test_socket = mtf::test_socket_file().c_str();
}

namespace
{

struct MockCursor : public mg::Cursor
{
    MOCK_METHOD1(set_image, void(std::shared_ptr<mg::CursorImage const> const&));
    MOCK_METHOD1(move_to, void(geom::Point));
};

struct NamedCursorImage : public mg::CursorImage
{
    NamedCursorImage(std::string const& theme, std::string const& name)
        : cursor_theme(theme),
          cursor_name(name)
    {
    }
    void const* as_argb_8888() const { return nullptr; }
    geom::Size size() const { return geom::Size{}; }
    std::string const cursor_theme;
    std::string const cursor_name;
};

struct StubCursorImages : public mg::CursorImages
{
   std::shared_ptr<mg::CursorImage> lookup_cursor(std::string const& theme, 
       std::string const& name, geom::Size const& /* size */)
   {
       return std::make_shared<NamedCursorImage>(theme, name);
   }
};

MATCHER(DefaultCursorImage, "")
{
    auto image = std::dynamic_pointer_cast<NamedCursorImage const>(arg);
    if (image->cursor_theme != "default")
        return false;
    if (image->cursor_name != "default")
        return false;
    return true;
}

MATCHER_P(CursorNamed, name, "")
{
   auto image = std::dynamic_pointer_cast<NamedCursorImage const>(arg);
   if (image->cursor_name != name)
       return false;
   return true;
}

struct CursorSettingClient : mtf::TestingClientConfiguration
{
    std::string const client_name;

    mtf::CrossProcessSync set_cursor_complete;
    mtf::CrossProcessSync client_may_exit;
    
    std::function<void(MirSurface*)> const set_cursor;

    CursorSettingClient(std::string const& client_name,
                        mtf::CrossProcessSync const& cursor_ready_fence,
                        mtf::CrossProcessSync const& client_may_exit_fence,
                        std::function<void(MirSurface*)> const& set_cursor)
        : client_name(client_name),
          set_cursor_complete(cursor_ready_fence),
          client_may_exit(client_may_exit_fence),
          set_cursor(set_cursor)
    {
    }

    void exec() override
    {
        auto connection = mir_connect_sync(mir_test_socket,
                                           client_name.c_str());
        
        ASSERT_TRUE(connection != NULL);
        MirSurfaceParameters const request_params =
            {
                client_name.c_str(),
                // For this fixture, we force geometry on server side
                0, 0,
                mir_pixel_format_abgr_8888,
                mir_buffer_usage_hardware,
                mir_display_output_id_invalid
            };
        auto surface = mir_connection_create_surface_sync(connection, &request_params);
        
        set_cursor(surface);
        set_cursor_complete.signal_ready();
        
        client_may_exit.wait_for_signal_ready_for();
        
        mir_surface_release_sync(surface);
        mir_connection_release(connection);
    }
};

typedef unsigned ClientCount;
struct CursorTestServerConfiguration : mtf::InputTestingServerConfiguration
{
    std::shared_ptr<msh::PlacementStrategy> placement_strategy;
    mtf::CrossProcessSync client_ready_fence;
    mtf::CrossProcessSync client_may_exit_fence;
    int const number_of_clients;

    std::function<void(MockCursor&, mt::WaitCondition&)> const expect_cursor_states;
    std::function<void(CursorTestServerConfiguration*)> const synthesize_cursor_motion;
    
    MockCursor cursor;

    CursorTestServerConfiguration(mtf::SurfaceGeometries surface_geometries_by_name,
                                  mtf::SurfaceDepths surface_depths_by_name,
                                  mtf::CrossProcessSync client_ready_fence,
                                  mtf::CrossProcessSync client_may_exit_fence,
                                  ClientCount const number_of_clients,
                                  std::function<void(MockCursor&, mt::WaitCondition&)> const& expect_cursor_states,
                                  std::function<void(CursorTestServerConfiguration*)> const& synthesize_cursor_motion)
        : placement_strategy(
              std::make_shared<mtf::DeclarativePlacementStrategy>(InputTestingServerConfiguration::the_shell_placement_strategy(),
                  surface_geometries_by_name, surface_depths_by_name)),
          client_ready_fence(client_ready_fence),
          client_may_exit_fence(client_may_exit_fence),
          number_of_clients(number_of_clients),
          expect_cursor_states(expect_cursor_states),
          synthesize_cursor_motion(synthesize_cursor_motion)
    {
    }
    
    std::shared_ptr<msh::PlacementStrategy> the_shell_placement_strategy() override
    {
        return placement_strategy;
    }
    
    std::shared_ptr<mg::Cursor> the_cursor() override
    {
        return mt::fake_shared(cursor);
    }
    
    std::shared_ptr<mg::CursorImages> the_cursor_images() override
    {
        return std::make_shared<StubCursorImages>();
    }
    
    void inject_input()
    {
        using namespace ::testing;

        for (int i = 1; i < number_of_clients + 1; i++)
            EXPECT_EQ(i, client_ready_fence.wait_for_signal_ready_for());

        mt::WaitCondition expectations_satisfied;
        
        // Clear any states applied during server initialization.
        Mock::VerifyAndClearExpectations(&cursor);
        expect_cursor_states(cursor, expectations_satisfied);

        synthesize_cursor_motion(this);
        expectations_satisfied.wait_for_at_most_seconds(60);

        EXPECT_CALL(cursor, set_image(_)).Times(AnyNumber()); // Client shutdown
        for (int i = 0; i < number_of_clients; i++)
            client_may_exit_fence.signal_ready();
    }
};

}

using TestClientCursorAPI = BespokeDisplayServerTestFixture;

// In this set we create a 1x1 client surface at the point (1,0). The client requests to disable the cursor
// over this surface. Since the cursor starts at (0,0) we when we move the cursor by (1,0) thus causing it
// to enter the bounds of the first surface, we should observe it being disabled.
TEST_F(TestClientCursorAPI, DISABLED_client_may_disable_cursor_over_surface)
{
    using namespace ::testing;

    std::string const test_client_name = "1";
    mtf::SurfaceGeometries client_geometries;
    client_geometries[test_client_name] = geom::Rectangle{geom::Point{geom::X{1}, geom::Y{0}},
                                                          geom::Size{geom::Width{1}, geom::Height{1}}};
    
    mtf::CrossProcessSync client_ready_fence, client_may_exit_fence;

    CursorTestServerConfiguration server_conf(
        client_geometries, mtf::SurfaceDepths(),
        client_ready_fence, client_may_exit_fence,
        ClientCount{1},
        [](MockCursor& cursor, mt::WaitCondition& expectations_satisfied)
        {
            EXPECT_CALL(cursor, set_image(Eq(nullptr))).Times(1)
                .WillOnce(mt::WakeUp(&expectations_satisfied));
        },
        [](CursorTestServerConfiguration *server)
        {
            server->fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 0));
        });
    launch_server_process(server_conf);
    
    CursorSettingClient client_conf(test_client_name, client_ready_fence, client_may_exit_fence,
        [](MirSurface *surface)
        {
            // Disable cursor
            MirCursorParameters cparams = { mir_false, NULL, NULL };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client_conf);
}

TEST_F(TestClientCursorAPI, DISABLED_cursor_restored_when_leaving_surface)
{
    using namespace ::testing;

    std::string const test_client_name = "1";
    mtf::SurfaceGeometries client_geometries;
    client_geometries[test_client_name] = geom::Rectangle{geom::Point{geom::X{1}, geom::Y{0}},
                                                          geom::Size{geom::Width{1}, geom::Height{1}}};
    
    mtf::CrossProcessSync client_ready_fence, client_may_exit_fence;

    CursorTestServerConfiguration server_conf(
        client_geometries, mtf::SurfaceDepths(),
        client_ready_fence, client_may_exit_fence,
        ClientCount{1},
        [](MockCursor& cursor, mt::WaitCondition& expectations_satisfied)
        {
            InSequence seq;
            EXPECT_CALL(cursor, set_image(Eq(nullptr))).Times(1);
            EXPECT_CALL(cursor, set_image(DefaultCursorImage())).Times(1)
                .WillOnce(mt::WakeUp(&expectations_satisfied));
        },
        [](CursorTestServerConfiguration *server)
        {
            server->fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 0));
            server->fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(2,0));
        });
    launch_server_process(server_conf);
    
    CursorSettingClient client_conf(test_client_name, client_ready_fence, client_may_exit_fence,
        [](MirSurface *surface)
        {
            // Disable cursor
            MirCursorParameters cparams = { mir_false, NULL, NULL };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client_conf);
}

TEST_F(TestClientCursorAPI, DISABLED_cursor_changed_when_crossing_surface_boundaries)
{
    using namespace ::testing;

    static std::string const test_client_name_1 = "1";
    static std::string const test_client_name_2 = "2";
    static std::string const client_1_cursor = test_client_name_1;
    static std::string const client_2_cursor = test_client_name_2;

    mtf::SurfaceGeometries client_geometries;
    client_geometries[test_client_name_1] = geom::Rectangle{geom::Point{geom::X{1}, geom::Y{0}},
                                                          geom::Size{geom::Width{1}, geom::Height{1}}};
    client_geometries[test_client_name_2] = geom::Rectangle{geom::Point{geom::X{2}, geom::Y{0}},
                                                            geom::Size{geom::Width{1}, geom::Height{1}}};
    
    mtf::CrossProcessSync client_ready_fence, client_may_exit_fence;

    CursorTestServerConfiguration server_conf(
        client_geometries, mtf::SurfaceDepths(),
        client_ready_fence, client_may_exit_fence,
        ClientCount{2},
        [](MockCursor& cursor, mt::WaitCondition& expectations_satisfied)
        {
            InSequence seq;
            EXPECT_CALL(cursor, set_image(CursorNamed(client_1_cursor))).Times(1);
            EXPECT_CALL(cursor, set_image(CursorNamed(client_2_cursor))).Times(1)
                .WillOnce(mt::WakeUp(&expectations_satisfied));
        },
        [](CursorTestServerConfiguration *server)
        {
            server->fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 0));
            server->fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 0));
        });
    launch_server_process(server_conf);
    
    CursorSettingClient client1_conf(test_client_name_1, client_ready_fence, client_may_exit_fence,
        [](MirSurface *surface)
        {
            MirCursorParameters cparams = { mir_true, "default", client_1_cursor.c_str() };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client1_conf);
    CursorSettingClient client2_conf(test_client_name_2, client_ready_fence, client_may_exit_fence,
        [](MirSurface *surface)
        {
            // Disable cursor
            MirCursorParameters cparams = { mir_true, "default", client_2_cursor.c_str() };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client2_conf);
}

TEST_F(TestClientCursorAPI, DISABLED_cursor_request_taken_from_top_surface)
{
    using namespace ::testing;

    static std::string const test_client_name_1 = "1";
    static std::string const test_client_name_2 = "2";
    static std::string const client_1_cursor = test_client_name_1;
    static std::string const client_2_cursor = test_client_name_2;

    mtf::SurfaceGeometries client_geometries;
    client_geometries[test_client_name_1] = geom::Rectangle{geom::Point{geom::X{1}, geom::Y{0}},
                                                          geom::Size{geom::Width{1}, geom::Height{1}}};
    client_geometries[test_client_name_1] = geom::Rectangle{geom::Point{geom::X{1}, geom::Y{0}},
                                                          geom::Size{geom::Width{1}, geom::Height{1}}};
    mtf::SurfaceDepths client_depths;
    client_depths[test_client_name_1] = ms::DepthId{0};
    client_depths[test_client_name_2] = ms::DepthId{1};
    
    mtf::CrossProcessSync client_ready_fence, client_may_exit_fence;

    CursorTestServerConfiguration server_conf(
        client_geometries, client_depths,
        client_ready_fence, client_may_exit_fence,
        ClientCount{2},
        [](MockCursor& cursor, mt::WaitCondition& expectations_satisfied)
        {
            InSequence seq;
            EXPECT_CALL(cursor, set_image(CursorNamed(client_2_cursor))).Times(1)
                .WillOnce(mt::WakeUp(&expectations_satisfied));
        },
        [](CursorTestServerConfiguration *server)
        {
            server->fake_event_hub->synthesize_event(mis::a_motion_event().with_movement(1, 0));
        });
    launch_server_process(server_conf);
    
    CursorSettingClient client1_conf(test_client_name_1, client_ready_fence, client_may_exit_fence,
        [](MirSurface *surface)
        {
            MirCursorParameters cparams = { mir_true, "default", client_1_cursor.c_str() };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client1_conf);
    CursorSettingClient client2_conf(test_client_name_2, client_ready_fence, client_may_exit_fence,
        [](MirSurface *surface)
        {
            // Disable cursor
            MirCursorParameters cparams = { mir_true, "default", client_2_cursor.c_str() };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client2_conf);
}

TEST_F(TestClientCursorAPI, DISABLED_cursor_request_applied_without_cursor_motion)
{
    using namespace ::testing;
    static std::string const test_client_name_1 = "1";
    static std::string const client_1_cursor = test_client_name_1;

    mtf::SurfaceGeometries client_geometries;
    client_geometries[test_client_name_1] = geom::Rectangle{geom::Point{geom::X{0}, geom::Y{0}},
                                                          geom::Size{geom::Width{1}, geom::Height{1}}};
    
    mtf::CrossProcessSync client_ready_fence, client_may_exit_fence;
    static mtf::CrossProcessSync client_may_change_cursor;

    CursorTestServerConfiguration server_conf(
        client_geometries, mtf::SurfaceDepths(),
        client_ready_fence, client_may_exit_fence,
        ClientCount{1},
        [](MockCursor& cursor, mt::WaitCondition& expectations_satisfied)
        {
            InSequence seq;
            EXPECT_CALL(cursor, set_image(CursorNamed(client_1_cursor))).Times(1);
            EXPECT_CALL(cursor, set_image(Eq(nullptr))).Times(1)
                .WillOnce(mt::WakeUp(&expectations_satisfied));
        },
        [](CursorTestServerConfiguration * /* server */)
        {
            client_may_change_cursor.signal_ready();
        });
    launch_server_process(server_conf);
    
    CursorSettingClient client1_conf(test_client_name_1, client_ready_fence, client_may_exit_fence,
        [&client_ready_fence](MirSurface *surface)
        {
            client_ready_fence.signal_ready();
            client_may_change_cursor.wait_for_signal_ready_for();
            MirCursorParameters cparams = { mir_true, "default", client_1_cursor.c_str() };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
            cparams = { mir_false, NULL, NULL };
            mir_wait_for(mir_surface_configure_cursor(surface, &cparams));
        });
    launch_client_process(client1_conf);
}
