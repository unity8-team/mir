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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */
#include "src/platform/graphics/mesa/platform.h"
#include "src/platform/graphics/mesa/display_buffer.h"
#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_gl.h"
#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_gbm.h"
#include "mir_test_doubles/stub_gl_config.h"
#include "mir_test_doubles/platform_factory.h"
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

class MesaDisplayBufferTest : public Test
{
public:
    MesaDisplayBufferTest()
     : bypassable_list{std::make_shared<FakeRenderable>(display_area)}
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
    NiceMock<MockGBM> mock_gbm;
    NiceMock<MockEGL> mock_egl;
    NiceMock<MockGL>  mock_gl;
    NiceMock<MockDRM> mock_drm; 
    gbm_bo*           fake_bo;
    gbm_bo_handle     fake_handle;
    UdevEnvironment   fake_devices;
    std::shared_ptr<MockKMSOutput> mock_kms_output;
    StubGLConfig gl_config;
    int const width{56};
    int const height{78};
    mir::geometry::Rectangle const display_area{{12,34}, {width,height}};
    mir::graphics::RenderableList const bypassable_list;

};

TEST_F(MesaDisplayBufferTest, UnrotatedViewAreaIsUntouched)
{
    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_EQ(display_area, db.view_area());
}

TEST_F(MesaDisplayBufferTest, NormalOrientationWithBypassableListCanBypass)
{
    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_TRUE(db.post_renderables_if_optimizable(bypassable_list));
}

TEST_F(MesaDisplayBufferTest, RotatedCannotBypass)
{
    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_right,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_FALSE(db.post_renderables_if_optimizable(bypassable_list));
}

TEST_F(MesaDisplayBufferTest, OrientationNotImplementedInternally)
{
    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_left,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_EQ(mir_orientation_left, db.orientation());
}

TEST_F(MesaDisplayBufferTest, NormalRotationConstructsNormalFb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB(_, width, height, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayBufferTest, LeftRotationConstructsTransposedFb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB(_, height, width, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_left,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayBufferTest, InvertedRotationConstructsNormalFb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB(_, width, height, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_inverted,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayBufferTest, RightRotationConstructsTransposedFb)
{
    EXPECT_CALL(mock_gbm, gbm_bo_get_user_data(_))
        .WillOnce(Return((void*)0));
    EXPECT_CALL(mock_drm, drmModeAddFB(_, height, width, _, _, _, _, _))
        .Times(1);

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {},
        nullptr,
        display_area,
        mir_orientation_right,
        gl_config,
        mock_egl.fake_egl_context);
}

TEST_F(MesaDisplayBufferTest, FirstPostFlipsButNoWait)
{
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(1);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(0);

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    db.post_update();
}

TEST_F(MesaDisplayBufferTest, WaitsForPageFlipOnSecondPost)
{
    InSequence seq;

    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(0);
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(1);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(1);
    EXPECT_CALL(*mock_kms_output, schedule_page_flip(_))
        .Times(1);
    EXPECT_CALL(*mock_kms_output, wait_for_page_flip())
        .Times(0);

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    db.post_update();
    db.post_update();
}

TEST_F(MesaDisplayBufferTest, SkipsBypassBecauseOfIncompatibleList)
{
    graphics::RenderableList list{
        std::make_shared<FakeRenderable>(display_area),
        std::make_shared<FakeRenderable>(geometry::Rectangle{{12, 34}, {1, 1}})
    };

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_FALSE(db.post_renderables_if_optimizable(list));
}

TEST_F(MesaDisplayBufferTest, SkipsBypassBecauseOfIncompatibleBypassBuffer)
{
    auto fullscreen = std::make_shared<FakeRenderable>(display_area);
    auto nonbypassable = std::make_shared<testing::NiceMock<MockBuffer>>();
    ON_CALL(*nonbypassable, can_bypass())
        .WillByDefault(Return(false));
    fullscreen->set_buffer(nonbypassable);
    graphics::RenderableList list{fullscreen};

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        display_area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_FALSE(db.post_renderables_if_optimizable(list));
}

TEST_F(MesaDisplayBufferTest, DoesNotUseAlpha)
{
    geometry::Rectangle const area{{12,34}, {56,78}};

    graphics::mesa::DisplayBuffer db(
        create_platform(),
        null_display_report(),
        {mock_kms_output},
        nullptr,
        area,
        mir_orientation_normal,
        gl_config,
        mock_egl.fake_egl_context);

    EXPECT_FALSE(db.uses_alpha());
}
