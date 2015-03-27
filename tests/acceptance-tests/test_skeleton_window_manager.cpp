/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/shell/skeleton_window_manager.h"

#include "mir/geometry/rectangle.h"

#include "mir_test_framework/connected_client_with_a_surface.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>


namespace msh = mir::shell;

namespace mt = mir::test;

using mir_test_framework::ConnectedClientWithASurface;
using namespace mir::geometry;
using namespace testing;

namespace
{
std::vector<Rectangle> const display_geometry { {{0, 0},{1, 1}}, {{0, 1},{1, 2}} };

struct SkeletonWindowManager : ConnectedClientWithASurface
{
    SkeletonWindowManager()
    {
        add_to_environment("MIR_SERVER_ENABLE_INPUT","off");
        initial_display_layout(display_geometry);
    }

    void SetUp() override
    {
        server.override_the_window_manager_builder([](msh::FocusController*)
            { return std::make_shared<msh::SkeletonWindowManager>(); });

        ConnectedClientWithASurface::SetUp();
    }
};
}

TEST_F(SkeletonWindowManager, allows_surface_creation)
{
    EXPECT_TRUE(mir_surface_is_valid(surface));
}

TEST_F(SkeletonWindowManager, does_not_size_surface_to_display)
{
    MirSurfaceParameters params;
    mir_surface_get_parameters(surface, &params);

    EXPECT_THAT(params.width, Gt(2));
    EXPECT_THAT(params.height, Gt(2));
}

TEST_F(SkeletonWindowManager, allows_all_states)
{
    for (auto state :
        {
            mir_surface_state_unknown,
            mir_surface_state_restored,
            mir_surface_state_minimized,
            mir_surface_state_fullscreen,
            mir_surface_state_maximized,
            mir_surface_state_vertmaximized,
            mir_surface_state_horizmaximized
        })
    {
        mir_wait_for(mir_surface_set_state(surface, state));

        EXPECT_THAT(mir_surface_get_state(surface), Eq(state));
    }
}

TEST_F(SkeletonWindowManager, ignores_output_selection)
{
    int const width = 13;
    int const height= 17;

    MirDisplayConfiguration* const config = mir_connection_create_display_config(connection);

    MirDisplayOutput* const output = config->outputs + 0;

    MirSurfaceSpec* const spec = mir_connection_create_spec_for_normal_surface(
        connection,
        width,
        height,
        mir_pixel_format_abgr_8888);

    mir_surface_spec_set_fullscreen_on_output(spec, output->output_id);

    MirSurface* const surface = mir_surface_create_sync(spec);
    mir_surface_spec_release(spec);

    MirSurfaceParameters params;
    mir_surface_get_parameters(surface, &params);

    MirDisplayMode* const mode = output->modes + output->current_mode;
    EXPECT_THAT(params.width, Ne(mode->horizontal_resolution));
    EXPECT_THAT(params.height, Ne(mode->vertical_resolution));

    EXPECT_THAT(params.width, Eq(width));
    EXPECT_THAT(params.height, Eq(height));

    mir_surface_release_sync(surface);
    mir_display_config_destroy(config);
}
