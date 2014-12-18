/*
 * Copyright © 2012 Canonical Ltd.
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

#include "mir/graphics/display_buffer.h"
#include "mir/graphics/display_configuration.h"
#include "mir/logging/logger.h"
#include "src/platforms/android/display.h"
#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/mock_display_report.h"
#include "mir_test_doubles/mock_display_device.h"
#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/stub_display_buffer.h"
#include "mir_test_doubles/stub_display_builder.h"
#include "mir_test_doubles/stub_gl_config.h"
#include "mir_test_doubles/mock_gl_config.h"
#include "mir_test_doubles/stub_gl_program_factory.h"
#include "mir/graphics/android/mir_native_window.h"
#include "mir_test_doubles/stub_driver_interpreter.h"

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <unordered_set>
#include <algorithm>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mtd=mir::test::doubles;
namespace geom=mir::geometry;

class Display : public ::testing::Test
{
public:
    Display()
        : dummy_display{mock_egl.fake_egl_display},
          dummy_context{mock_egl.fake_egl_context},
          dummy_config{mock_egl.fake_configs[0]},
          null_display_report{mir::report::null_display_report()},
          stub_db_factory{std::make_shared<mtd::StubDisplayBuilder>()},
          stub_gl_config{std::make_shared<mtd::StubGLConfig>()},
          stub_gl_program_factory{std::make_shared<mtd::StubGLProgramFactory>()}
    {
    }

protected:
    testing::NiceMock<mtd::MockEGL> mock_egl;
    EGLDisplay const dummy_display;
    EGLContext const dummy_context;
    EGLConfig const dummy_config;

    std::shared_ptr<mg::DisplayReport> const null_display_report;
    std::shared_ptr<mtd::StubDisplayBuilder> const stub_db_factory;
    std::shared_ptr<mtd::StubGLConfig> const stub_gl_config;
    std::shared_ptr<mtd::StubGLProgramFactory> const stub_gl_program_factory;
};

TEST_F(Display, creation_creates_egl_resources_properly)
{
    using namespace testing;
    EGLSurface fake_surface = (EGLSurface) 0x715;
    EGLint const expected_pbuffer_attr[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    EGLint const expected_context_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };

    //on constrution
    EXPECT_CALL(mock_egl, eglGetDisplay(EGL_DEFAULT_DISPLAY))
        .Times(1)
        .WillOnce(Return(dummy_display));
    EXPECT_CALL(mock_egl, eglInitialize(dummy_display, _, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<1>(1), SetArgPointee<2>(4), Return(EGL_TRUE)));
    EXPECT_CALL(mock_egl, eglCreateContext(
        dummy_display, _, EGL_NO_CONTEXT, mtd::AttrMatches(expected_context_attr)))
        .Times(1)
        .WillOnce(Return(dummy_context));
    EXPECT_CALL(mock_egl, eglCreatePbufferSurface(
        dummy_display, _, mtd::AttrMatches(expected_pbuffer_attr)))
        .Times(1)
        .WillOnce(Return(fake_surface));
    EXPECT_CALL(mock_egl, eglMakeCurrent(dummy_display, fake_surface, fake_surface, dummy_context))
        .Times(1);

    //on destruction
    EXPECT_CALL(mock_egl, eglGetCurrentContext())
        .Times(1)
        .WillOnce(Return(dummy_context));
    EXPECT_CALL(mock_egl, eglMakeCurrent(dummy_display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT))
        .Times(1);
    EXPECT_CALL(mock_egl, eglDestroySurface(dummy_display, fake_surface))
        .Times(1);
    EXPECT_CALL(mock_egl, eglDestroyContext(dummy_display, dummy_context))
        .Times(1);
    EXPECT_CALL(mock_egl, eglTerminate(dummy_display))
        .Times(1);

    mga::Display display(
        stub_db_factory,
        stub_gl_program_factory,
        stub_gl_config,
        null_display_report);
}

TEST_F(Display, selects_usable_configuration)
{
    using namespace testing;
    int const incorrect_visual_id = 2;
    int const correct_visual_id = 1;
    EGLint const num_cfgs = 45;
    EGLint const expected_cfg_attr [] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE };
    EGLConfig selected_config;
    EGLConfig cfgs[45];
    for(auto i = 0; i < num_cfgs; i++)
        cfgs[i] = reinterpret_cast<EGLConfig>(i);
    int config_to_select_index = 37;
    EGLConfig correct_config = cfgs[config_to_select_index];

    ON_CALL(mock_egl, eglGetConfigAttrib(_, _, EGL_NATIVE_VISUAL_ID, _))
        .WillByDefault(DoAll(SetArgPointee<3>(incorrect_visual_id), Return(EGL_TRUE)));
    ON_CALL(mock_egl, eglGetConfigAttrib(dummy_display, correct_config, EGL_NATIVE_VISUAL_ID,_))
        .WillByDefault(DoAll(SetArgPointee<3>(correct_visual_id), Return(EGL_TRUE)));
    ON_CALL(mock_egl, eglCreateContext(_,_,_,_))
        .WillByDefault(DoAll(SaveArg<1>(&selected_config),Return(dummy_context)));

    auto config_filler = [&]
    (EGLDisplay, EGLint const*, EGLConfig* out_cfgs, EGLint, EGLint* out_num_cfgs) -> EGLBoolean
    {
        memcpy(out_cfgs, cfgs, sizeof(EGLConfig) * num_cfgs);
        *out_num_cfgs = num_cfgs;
        return EGL_TRUE;
    };

    EXPECT_CALL(mock_egl, eglGetConfigs(dummy_display, NULL, 0, _))
        .Times(1)
        .WillOnce(DoAll(SetArgPointee<3>(num_cfgs), Return(EGL_TRUE)));
    EXPECT_CALL(mock_egl, eglChooseConfig(dummy_display, mtd::AttrMatches(expected_cfg_attr),_,num_cfgs,_))
        .Times(1)
        .WillOnce(Invoke(config_filler));

    mga::Display display(
        stub_db_factory,
        stub_gl_program_factory,
        stub_gl_config,
        null_display_report);
    EXPECT_EQ(correct_config, selected_config);
}

TEST_F(Display, respects_gl_config)
{
    using namespace testing;

    auto const mock_gl_config = std::make_shared<mtd::MockGLConfig>();
    EGLint const depth_bits{24};
    EGLint const stencil_bits{8};

    EXPECT_CALL(*mock_gl_config, depth_buffer_bits())
        .WillOnce(Return(depth_bits));
    EXPECT_CALL(*mock_gl_config, stencil_buffer_bits())
        .WillOnce(Return(stencil_bits));

    EXPECT_CALL(mock_egl,
                eglChooseConfig(
                    _,
                    AllOf(mtd::EGLConfigContainsAttrib(EGL_DEPTH_SIZE, depth_bits),
                          mtd::EGLConfigContainsAttrib(EGL_STENCIL_SIZE, stencil_bits)),
                    _,_,_));

    mga::Display display(
        stub_db_factory,
        stub_gl_program_factory,
        mock_gl_config,
        null_display_report);
}

TEST_F(Display, logs_creation_events)
{
    using namespace testing;

    auto const mock_display_report = std::make_shared<mtd::MockDisplayReport>();

    EXPECT_CALL(*mock_display_report, report_successful_setup_of_native_resources())
        .Times(1);
    EXPECT_CALL(*mock_display_report, report_egl_configuration(_,_))
        .Times(1);
    EXPECT_CALL(*mock_display_report, report_successful_egl_make_current_on_construction())
        .Times(1);
    EXPECT_CALL(*mock_display_report, report_successful_display_construction())
        .Times(1);

    mga::Display display(
        stub_db_factory,
        stub_gl_program_factory,
        stub_gl_config,
        mock_display_report);
}

TEST_F(Display, throws_on_eglMakeCurrent_failure)
{
    using namespace testing;

    auto const mock_display_report = std::make_shared<NiceMock<mtd::MockDisplayReport>>();

    EXPECT_CALL(*mock_display_report, report_successful_setup_of_native_resources())
        .Times(1);
    EXPECT_CALL(mock_egl, eglMakeCurrent(dummy_display, _, _, _))
        .Times(1)
        .WillOnce(Return(EGL_FALSE));
    EXPECT_CALL(*mock_display_report, report_successful_egl_make_current_on_construction())
        .Times(0);
    EXPECT_CALL(*mock_display_report, report_successful_display_construction())
        .Times(0);

    EXPECT_THROW({
        mga::Display display(
            stub_db_factory,
            stub_gl_program_factory,
            stub_gl_config,
            mock_display_report);
    }, std::runtime_error);
}

TEST_F(Display, logs_error_because_of_surface_creation_failure)
{
    using namespace testing;

    auto const mock_display_report = std::make_shared<mtd::MockDisplayReport>();

    EXPECT_CALL(*mock_display_report, report_successful_setup_of_native_resources())
        .Times(0);
    EXPECT_CALL(*mock_display_report, report_successful_egl_make_current_on_construction())
        .Times(0);
    EXPECT_CALL(*mock_display_report, report_successful_display_construction())
        .Times(0);

    EXPECT_CALL(mock_egl, eglCreatePbufferSurface(_,_,_))
        .Times(1)
        .WillOnce(Return(EGL_NO_SURFACE));

    EXPECT_THROW({
        mga::Display display(
            stub_db_factory,
            stub_gl_program_factory,
            stub_gl_config,
            mock_display_report);
    }, std::runtime_error);
}

TEST_F(Display, configures_display_buffer)
{
    using namespace testing;
    mga::Display display(
        stub_db_factory,
        stub_gl_program_factory,
        stub_gl_config,
        null_display_report);

    auto configuration = display.configuration();
    configuration->for_each_output([&](mg::UserDisplayConfigurationOutput& output)
    {
        output.power_mode = mir_power_mode_on;
    });
    display.configure(*configuration);

    configuration->for_each_output([&](mg::UserDisplayConfigurationOutput& output)
    {
        output.power_mode = mir_power_mode_standby;
    });
    display.configure(*configuration);

    configuration->for_each_output([&](mg::UserDisplayConfigurationOutput& output)
    {
        output.power_mode = mir_power_mode_off;
    });
    display.configure(*configuration);

    configuration->for_each_output([&](mg::UserDisplayConfigurationOutput& output)
    {
        output.power_mode = mir_power_mode_suspend;
    });
    display.configure(*configuration);
}

//we only have single display and single mode on android for the time being
TEST_F(Display, supports_one_output_configuration)
{
    mga::Display display(
        stub_db_factory,
        stub_gl_program_factory,
        stub_gl_config,
        null_display_report);
    auto config = display.configuration();

    size_t num_configs = 0;
    config->for_each_output([&](mg::DisplayConfigurationOutput const&)
    {
        num_configs++;
    });

    EXPECT_EQ(1u, num_configs);
}
