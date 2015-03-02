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
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir/scene/surface_creation_parameters.h"
#include "mir/scene/placement_strategy.h"

#include "mir_test_framework/headless_test.h"
#include "mir_test_framework/any_surface.h"

#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/debug/surface.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace ms = mir::scene;

namespace mtf = mir_test_framework;

namespace
{
class SimpleConfigurablePlacementStrategy : public ms::PlacementStrategy
{
public:
    ms::SurfaceCreationParameters place(ms::Session const& /*session*/,
                                        ms::SurfaceCreationParameters const& request_parameters) override
    {
        return ms::SurfaceCreationParameters(request_parameters)
            .of_position(placement.top_left)
            .of_size(placement.size);
    }

    mir::geometry::Rectangle placement{{0, 0}, {100, 100}};
};

char const* const debugenv = "MIR_SERVER_DEBUG";

void dont_kill_me_bro(MirConnection* /*unused*/, MirLifecycleState /*unused*/, void* /*unused*/)
{
}

class DebugAPI : public mtf::HeadlessTest
{
public:

    void SetUp() override
    {
        add_to_environment("MIR_SERVER_NO_FILE", "");

        server.override_the_placement_strategy([&]{ return placement_strategy; });
        mtf::HeadlessTest::SetUp();
    }

    void set_surface_placement(mir::geometry::Rectangle const& where)
    {
        placement_strategy->placement = where;
    }

    void start_server_with_debug(bool debug)
    {
        if (debug)
        {
            add_to_environment(debugenv, "");
        }
        else
        {
            add_to_environment(debugenv, nullptr);
        }

        start_server();

        connection = mir_connect_sync(new_connection().c_str(), __PRETTY_FUNCTION__);
        if (!mir_connection_is_valid(connection))
        {
            throw std::runtime_error{std::string{"Failed to connect to test server:"} +
                                     mir_connection_get_error_message(connection)};
        }
        mir_connection_set_lifecycle_event_callback(connection, dont_kill_me_bro, nullptr);
    }

    void TearDown() override
    {
        if (connection)
        {
            mir_connection_release(connection);
        }

        stop_server();
        mtf::HeadlessTest::TearDown();
    }

    MirConnection* connection{nullptr};

private:
    std::shared_ptr<SimpleConfigurablePlacementStrategy> const placement_strategy
        {std::make_shared<SimpleConfigurablePlacementStrategy>()};
};
}

TEST_F(DebugAPI, translates_surface_coordinates_to_screen_coordinates)
{
    start_server_with_debug(true);

    mir::geometry::Rectangle surface_location{{200, 100}, {800, 600}};

    set_surface_placement(surface_location);

    auto surf = mtf::make_any_surface(connection);
    ASSERT_TRUE(mir_surface_is_valid(surf));

    int screen_x, screen_y, x, y;
    x = 35, y = 21;

    ASSERT_TRUE(mir_debug_surface_coords_to_screen(surf, x, y, &screen_x, &screen_y));
    EXPECT_EQ(x + surface_location.top_left.x.as_int(), screen_x);
    EXPECT_EQ(y + surface_location.top_left.y.as_int(), screen_y);

    mir_surface_release_sync(surf);

    surface_location.top_left = {100, 250};

    set_surface_placement(surface_location);

    surf = mtf::make_any_surface(connection);
    ASSERT_TRUE(mir_surface_is_valid(surf));

    ASSERT_TRUE(mir_debug_surface_coords_to_screen(surf, x, y, &screen_x, &screen_y));
    EXPECT_EQ(x + surface_location.top_left.x.as_int(), screen_x);
    EXPECT_EQ(y + surface_location.top_left.y.as_int(), screen_y);

    mir_surface_release_sync(surf);
}

TEST_F(DebugAPI, is_unavaliable_when_server_not_started_with_debug)
{
    start_server_with_debug(false);

    auto surf = mtf::make_any_surface(connection);
    ASSERT_TRUE(mir_surface_is_valid(surf));

    int screen_x, screen_y;

    EXPECT_FALSE(mir_debug_surface_coords_to_screen(surf, 0, 0, &screen_x, &screen_y));

    mir_surface_release_sync(surf);
}
