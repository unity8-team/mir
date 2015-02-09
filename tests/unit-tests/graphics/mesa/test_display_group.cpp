/*
 * Copyright © 2015 Canonical Ltd.
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

#include "src/platforms/mesa/server/platform.h"
#include "src/platforms/mesa/server/display_group.h"
#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_gl.h"
#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_gbm.h"
#include "mir_test_doubles/stub_gl_config.h"
#include "mir_test_doubles/platform_factory.h"
#include "mir_test_doubles/stub_gbm_native_buffer.h"
#include "mir_test_framework/udev_environment.h"
#include "mir_test_doubles/fake_renderable.h"
#include "mock_kms_output.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <gbm.h>

using namespace testing;
using namespace mir;
using namespace std;
using namespace mir::test;
using namespace mir::test::doubles;
using namespace mir_test_framework;
using namespace mir::graphics;
using mir::report::null_display_report;

class MesaDisplayGroup : public Test
{
public:
    MesaDisplayGroup()
        : mock_bypassable_buffer{std::make_shared<NiceMock<MockBuffer>>()}
        , stub_gbm_native_buffer{
             std::make_shared<StubGBMNativeBuffer>(display_area.size)}
    {
        ON_CALL(mock_egl, eglChooseConfig(_,_,_,1,_))
            .WillByDefault(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                                 SetArgPointee<4>(1),
                                 Return(EGL_TRUE)));

        mock_egl.provide_egl_extensions();
        mock_gl.provide_gles_extensions();

        fake_bo = reinterpret_cast<gbm_bo*>(123);
        ON_CALL(mock_gbm, gbm_surface_lock_front_buffer(_))
            .WillByDefault(Return(fake_bo));
        fake_handle.u32 = 123;
        ON_CALL(mock_gbm, gbm_bo_get_handle(_))
            .WillByDefault(Return(fake_handle));
        ON_CALL(mock_gbm, gbm_bo_get_stride(_))
            .WillByDefault(Return(456));

        fake_devices.add_standard_device("standard-drm-devices");

        mock_kms_output = std::make_shared<NiceMock<MockKMSOutput>>();
        ON_CALL(*mock_kms_output, set_crtc(_))
            .WillByDefault(Return(true));
        ON_CALL(*mock_kms_output, schedule_page_flip(_))
            .WillByDefault(Return(true));
    }

    // The platform has an implicit dependency on mock_gbm etc so must be
    // reconstructed locally to ensure its lifetime is shorter than mock_gbm.
    shared_ptr<graphics::mesa::Platform> create_platform()
    {
        return mir::test::doubles::create_mesa_platform_with_null_dependencies();
    }

protected:
    int const width{56};
    int const height{78};
    mir::geometry::Rectangle const display_area{{12,34}, {width,height}};
    NiceMock<MockGBM> mock_gbm;
    NiceMock<MockEGL> mock_egl;
    NiceMock<MockGL>  mock_gl;
    NiceMock<MockDRM> mock_drm; 
    std::shared_ptr<MockBuffer> mock_bypassable_buffer;
    std::shared_ptr<mesa::GBMNativeBuffer> stub_gbm_native_buffer;
    gbm_bo*           fake_bo;
    gbm_bo_handle     fake_handle;
    UdevEnvironment   fake_devices;
    std::shared_ptr<MockKMSOutput> mock_kms_output;
    StubGLConfig gl_config;

};

TEST_F(MesaDisplayGroup, clone_mode_first_flip_flips_but_no_wait)
{
    // Ensure clone mode can do multiple page flips in parallel without
    // blocking on either (at least till the second post)
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(2);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(0);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {mock_kms_output, mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    dg.post();
}

TEST_F(MesaDisplayGroup, single_mode_first_post_flips_with_wait)
{
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(1);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(1);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    dg.post();
}

TEST_F(MesaDisplayGroup, clone_mode_waits_for_page_flip_on_second_flip)
{
    InSequence seq;

    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(0);
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(2);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(2);
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(2);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(0);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {mock_kms_output, mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    dg.post();
    dg.post();
}

TEST_F(MesaDisplayGroup, normal_rotation_constructs_normal_fb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB2(_, width, height, _, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayGroup, left_rotation_constructs_transposed_fb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB2(_, height, width, _, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_left,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayGroup, inverted_rotation_constructs_normal_fb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB2(_, width, height, _, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_inverted,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayGroup, right_rotation_constructs_transposed_fb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB2(_, height, width, _, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayGroup dg(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_right,
        gl_config,
        mock_egl.fake_egl_context);
}
