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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */
#include <boost/throw_exception.hpp>
#include "src/platforms/mesa/server/platform.h"
#include "src/platforms/mesa/server/display.h"
#include "src/platforms/mesa/server/virtual_terminal.h"
#include "src/server/report/logging/display_report.h"
#include "mir/logging/logger.h"
#include "mir/graphics/display_buffer.h"
#include "src/server/graphics/default_display_configuration_policy.h"
#include "mir/time/steady_clock.h"
#include "mir/glib_main_loop.h"

#include "mir_test_doubles/mock_egl.h"
#include "mir_test_doubles/mock_gl.h"
#include "mir_test_doubles/advanceable_clock.h"
#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/mock_display_report.h"
#include "mir_test_doubles/null_virtual_terminal.h"
#include "mir_test_doubles/stub_gl_config.h"
#include "mir_test_doubles/mock_gl_config.h"
#include "mir_test_doubles/platform_factory.h"
#include "mir_test_doubles/mock_virtual_terminal.h"
#include "mir_test_doubles/null_emergency_cleanup.h"

#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"

#include "mir_test_framework/udev_environment.h"
#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <memory>
#include <stdexcept>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace mg=mir::graphics;
namespace mgm=mir::graphics::mesa;
namespace ml=mir::logging;
namespace mrl=mir::report::logging;
namespace mtd=mir::test::doubles;
namespace mtf=mir_test_framework;
namespace mr=mir::report;

namespace
{
struct MockLogger : public ml::Logger
{
    MOCK_METHOD3(log,
                 void(ml::Severity, const std::string&, const std::string&));

    ~MockLogger() noexcept(true) {}
};

class MockEventRegister : public mg::EventHandlerRegister
{
public:
    MOCK_METHOD2(register_signal_handler,
                 void(std::initializer_list<int>,
                 std::function<void(int)> const&));
    MOCK_METHOD3(register_fd_handler,
                 void(std::initializer_list<int>,
                 void const*, std::function<void(int)> const&));
    MOCK_METHOD1(unregister_fd_handler,
                 void(void const*));

};


class MesaDisplayTest : public ::testing::Test
{
public:
    MesaDisplayTest() :
        mock_report{std::make_shared<testing::NiceMock<mtd::MockDisplayReport>>()},
        null_report{mr::null_display_report()}
    {
        using namespace testing;
        ON_CALL(mock_egl, eglChooseConfig(_,_,_,1,_))
        .WillByDefault(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                             SetArgPointee<4>(1),
                             Return(EGL_TRUE)));

        mock_egl.provide_egl_extensions();
        mock_gl.provide_gles_extensions();
        /*
         * Silence uninteresting calls called when cleaning up resources in
         * the MockGBM destructor, and which are not handled by NiceMock<>.
         */
        EXPECT_CALL(mock_gbm, gbm_bo_get_device(_))
        .Times(AtLeast(0));
        EXPECT_CALL(mock_gbm, gbm_device_get_fd(_))
        .Times(AtLeast(0));

        fake_devices.add_standard_device("standard-drm-devices");
    }

    std::shared_ptr<mgm::Platform> create_platform()
    {
        return mtd::create_mesa_platform_with_null_dependencies();
    }

    std::shared_ptr<mgm::Display> create_display(
        std::shared_ptr<mgm::Platform> const& platform)
    {
        return std::make_shared<mgm::Display>(
            platform,
            std::make_shared<mg::DefaultDisplayConfigurationPolicy>(),
            std::make_shared<mtd::StubGLConfig>(),
            null_report);
    }

    void setup_post_update_expectations()
    {
        using namespace testing;

        EXPECT_CALL(mock_egl, eglSwapBuffers(mock_egl.fake_egl_display,
                                             mock_egl.fake_egl_surface))
            .Times(Exactly(2));

        EXPECT_CALL(mock_gbm, gbm_surface_lock_front_buffer(mock_gbm.fake_gbm.surface))
            .Times(Exactly(2))
            .WillOnce(Return(fake.bo1))
            .WillOnce(Return(fake.bo2));

        EXPECT_CALL(mock_gbm, gbm_bo_get_handle(fake.bo1))
            .Times(Exactly(1))
            .WillOnce(Return(fake.bo_handle1));

        EXPECT_CALL(mock_gbm, gbm_bo_get_handle(fake.bo2))
            .Times(Exactly(1))
            .WillOnce(Return(fake.bo_handle2));

        EXPECT_CALL(mock_drm, drmModeAddFB2(mock_drm.fake_drm.fd(),
                                            _, _, _,
                                            Pointee(fake.bo_handle1.u32),
                                            _, _, _, _))
            .Times(Exactly(1))
            .WillOnce(DoAll(SetArgPointee<7>(fake.fb_id1), Return(0)));

        EXPECT_CALL(mock_drm, drmModeAddFB2(mock_drm.fake_drm.fd(),
                                            _, _, _,
                                            Pointee(fake.bo_handle2.u32),
                                            _, _, _, _))
            .Times(Exactly(1))
            .WillOnce(DoAll(SetArgPointee<7>(fake.fb_id2), Return(0)));
    }

    uint32_t get_connected_connector_id()
    {
        auto drm_res = mock_drm.fake_drm.resources_ptr();

        for (int i = 0; i < drm_res->count_connectors; i++)
        {
            auto connector = mock_drm.fake_drm.find_connector(drm_res->connectors[i]);
            if (connector->connection == DRM_MODE_CONNECTED)
                return connector->connector_id;
        }

        return 0;
    }

    uint32_t get_connected_crtc_id()
    {
        auto connector_id = get_connected_connector_id();
        auto connector = mock_drm.fake_drm.find_connector(connector_id);

        if (connector)
        {
            auto encoder = mock_drm.fake_drm.find_encoder(connector->encoder_id);
            if (encoder)
                return encoder->crtc_id;
        }

        return 0;
    }

    struct FakeData {
        FakeData()
            : bo1{reinterpret_cast<gbm_bo*>(0xabcd)},
              bo2{reinterpret_cast<gbm_bo*>(0xabce)},
              fb_id1{66}, fb_id2{67}, crtc()
        {
            bo_handle1.u32 = 0x1234;
            bo_handle2.u32 = 0x1235;
            crtc.buffer_id = 88;
            crtc.crtc_id = 565;
        }

        gbm_bo* bo1;
        gbm_bo* bo2;
        uint32_t fb_id1;
        uint32_t fb_id2;
        gbm_bo_handle bo_handle1;
        gbm_bo_handle bo_handle2;
        drmModeCrtc crtc;
    } fake;

    ::testing::NiceMock<mtd::MockEGL> mock_egl;
    ::testing::NiceMock<mtd::MockGL> mock_gl;
    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;
    std::shared_ptr<testing::NiceMock<mtd::MockDisplayReport>> const mock_report;
    std::shared_ptr<mg::DisplayReport> const null_report;
    mtf::UdevEnvironment fake_devices;
};

}

TEST_F(MesaDisplayTest, create_display)
{
    using namespace testing;

    auto const connector_id = get_connected_connector_id();
    auto const crtc_id = get_connected_crtc_id();

    /* To display a gbm surface, the MesaDisplay should... */

    /* Create a gbm surface to use as the frame buffer */
    EXPECT_CALL(mock_gbm, gbm_surface_create(mock_gbm.fake_gbm.device,_,_,_,_))
        .Times(Exactly(1));

    /* Create an EGL window surface backed by the gbm surface */
    EXPECT_CALL(mock_egl, eglCreateWindowSurface(mock_egl.fake_egl_display,
                                                 mock_egl.fake_configs[0],
                                                 (EGLNativeWindowType)mock_gbm.fake_gbm.surface, _))
        .Times(Exactly(1));

    /* Swap the EGL window surface to bring the back buffer to the front */
    EXPECT_CALL(mock_egl, eglSwapBuffers(mock_egl.fake_egl_display,
                                         mock_egl.fake_egl_surface))
        .Times(Exactly(1));

    /* Get the gbm_bo object corresponding to the front buffer */
    EXPECT_CALL(mock_gbm, gbm_surface_lock_front_buffer(mock_gbm.fake_gbm.surface))
        .Times(Exactly(1))
        .WillOnce(Return(fake.bo1));

    /* Get the DRM buffer handle associated with the gbm_bo */
    EXPECT_CALL(mock_gbm, gbm_bo_get_handle(fake.bo1))
        .Times(Exactly(1))
        .WillOnce(Return(fake.bo_handle1));

    /* Create a a DRM FB with the DRM buffer attached */
    EXPECT_CALL(mock_drm, drmModeAddFB2(mock_drm.fake_drm.fd(),
                                       _, _, _,
                                       Pointee(fake.bo_handle1.u32),
                                       _, _, _, _))
        .Times(Exactly(1))
        .WillOnce(DoAll(SetArgPointee<7>(fake.fb_id1), Return(0)));

    /* Display the DRM FB (first expectation is for cleanup) */
    EXPECT_CALL(mock_drm, drmModeSetCrtc(mock_drm.fake_drm.fd(),
                                         crtc_id, Ne(fake.fb_id1),
                                         _, _,
                                         Pointee(connector_id),
                                         _, _))
        .Times(AtLeast(0));

    EXPECT_CALL(mock_drm, drmModeSetCrtc(mock_drm.fake_drm.fd(),
                                         crtc_id, fake.fb_id1,
                                         _, _,
                                         Pointee(connector_id),
                                         _, _))
        .Times(Exactly(1))
        .WillOnce(Return(0));


    auto display = create_display(create_platform());
}

TEST_F(MesaDisplayTest, reset_crtc_on_destruction)
{
    using namespace testing;

    auto const connector_id = get_connected_connector_id();
    auto const crtc_id = get_connected_crtc_id();
    uint32_t const fb_id{66};

    /* Create DRM FBs */
    EXPECT_CALL(mock_drm, drmModeAddFB2(mock_drm.fake_drm.fd(),
                                        _, _, _, _, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<7>(fb_id), Return(0)));


    {
        InSequence s;

        /* crtc is set */
        EXPECT_CALL(mock_drm, drmModeSetCrtc(mock_drm.fake_drm.fd(),
                                             crtc_id, fb_id,
                                             _, _,
                                             Pointee(connector_id),
                                             _, _))
            .Times(AtLeast(1));

        /* crtc is reset */
        EXPECT_CALL(mock_drm, drmModeSetCrtc(mock_drm.fake_drm.fd(),
                                             crtc_id, Ne(fb_id),
                                             _, _,
                                             Pointee(connector_id),
                                             _, _))
            .Times(1);
    }

    auto display = create_display(create_platform());
}

TEST_F(MesaDisplayTest, create_display_drm_failure)
{
    using namespace testing;

    EXPECT_CALL(mock_drm, open(_,_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(Return(-1));

    EXPECT_CALL(mock_drm, drmClose(_))
        .Times(Exactly(0));

    EXPECT_THROW(
    {
        auto display = create_display(create_platform());
    }, std::runtime_error);
}

TEST_F(MesaDisplayTest, create_display_kms_failure)
{
    using namespace testing;

    auto platform = create_platform();

    Mock::VerifyAndClearExpectations(&mock_drm);

    EXPECT_CALL(mock_drm, drmModeGetResources(_))
        .Times(Exactly(1))
        .WillOnce(Return(reinterpret_cast<drmModeRes*>(0)));

    EXPECT_CALL(mock_drm, drmModeFreeResources(_))
        .Times(Exactly(0));

    EXPECT_CALL(mock_drm, drmClose(_))
        .Times(Exactly(1));

    EXPECT_THROW({
        auto display = create_display(platform);
    }, std::runtime_error) << "Expected that c'tor of mgm::Display throws";
}

TEST_F(MesaDisplayTest, create_display_gbm_failure)
{
    using namespace testing;

    EXPECT_CALL(mock_gbm, gbm_create_device(_))
        .Times(Exactly(1))
        .WillOnce(Return(reinterpret_cast<gbm_device*>(0)));

    EXPECT_CALL(mock_gbm, gbm_device_destroy(_))
        .Times(Exactly(0));

    EXPECT_CALL(mock_drm, drmClose(_))
        .Times(Exactly(1));

    EXPECT_THROW({
        auto platform = create_platform();
    }, std::runtime_error) << "Expected c'tor of Platform to throw an exception";
}

namespace
{

ACTION_P(QueuePageFlipEvent, write_drm_fd)
{
    EXPECT_EQ(1, write(write_drm_fd, "a", 1));
}

ACTION_P(InvokePageFlipHandler, param)
{
    int const dont_care{0};
    char dummy;

    arg1->page_flip_handler(dont_care, dont_care, dont_care, dont_care, *param);
    ASSERT_EQ(1, read(arg0, &dummy, 1));
}

}

TEST_F(MesaDisplayTest, post_update)
{
    using namespace testing;

    auto const crtc_id = get_connected_crtc_id();
    void* user_data{nullptr};

    setup_post_update_expectations();

    {
        InSequence s;

        /* Flip the new FB */
        EXPECT_CALL(mock_drm, drmModePageFlip(mock_drm.fake_drm.fd(),
                                              crtc_id,
                                              fake.fb_id2,
                                              _, _))
            .Times(Exactly(1))
            .WillOnce(DoAll(QueuePageFlipEvent(mock_drm.fake_drm.write_fd()),
                            SaveArg<4>(&user_data),
                            Return(0)));

        /* Handle the flip event */
        EXPECT_CALL(mock_drm, drmHandleEvent(mock_drm.fake_drm.fd(), _))
            .Times(1)
            .WillOnce(DoAll(InvokePageFlipHandler(&user_data), Return(0)));

        /* Release last_flipped_bufobj (at destruction time) */
        EXPECT_CALL(mock_gbm, gbm_surface_release_buffer(mock_gbm.fake_gbm.surface, fake.bo1))
            .Times(Exactly(1));
        /* Release scheduled_bufobj (at destruction time) */
        EXPECT_CALL(mock_gbm, gbm_surface_release_buffer(mock_gbm.fake_gbm.surface, fake.bo2))
            .Times(Exactly(1));
    }


    auto display = create_display(create_platform());

    display->for_each_display_sync_group([](mg::DisplaySyncGroup& group) {
        group.for_each_display_buffer([](mg::DisplayBuffer& db) {
            db.gl_swap_buffers();
        });
        group.post();
    });
}

TEST_F(MesaDisplayTest, post_update_flip_failure)
{
    using namespace testing;

    auto const crtc_id = get_connected_crtc_id();

    setup_post_update_expectations();

    {
        InSequence s;

        /* New FB flip failure */
        EXPECT_CALL(mock_drm, drmModePageFlip(mock_drm.fake_drm.fd(),
                                              crtc_id,
                                              fake.fb_id2,
                                              _, _))
            .Times(Exactly(1))
            .WillOnce(Return(-1));

        /* Release failed bufobj */
        EXPECT_CALL(mock_gbm, gbm_surface_release_buffer(mock_gbm.fake_gbm.surface, fake.bo2))
            .Times(Exactly(1));

        /* Release scheduled_bufobj (at destruction time) */
        EXPECT_CALL(mock_gbm, gbm_surface_release_buffer(mock_gbm.fake_gbm.surface, fake.bo1))
            .Times(Exactly(1));
    }

    EXPECT_THROW(
    {
        auto display = create_display(create_platform());
        display->for_each_display_sync_group([](mg::DisplaySyncGroup& group) {
            group.for_each_display_buffer([](mg::DisplayBuffer& db) {
                db.gl_swap_buffers();
            });
            group.post();
        });
    }, std::runtime_error);
}

TEST_F(MesaDisplayTest, successful_creation_of_display_reports_successful_setup_of_native_resources)
{
    using namespace ::testing;

    EXPECT_CALL(
        *mock_report,
        report_successful_setup_of_native_resources()).Times(Exactly(1));
    EXPECT_CALL(
        *mock_report,
        report_successful_egl_make_current_on_construction()).Times(Exactly(1));

    EXPECT_CALL(
        *mock_report,
        report_successful_egl_buffer_swap_on_construction()).Times(Exactly(1));

    EXPECT_CALL(
        *mock_report,
        report_successful_drm_mode_set_crtc_on_construction()).Times(Exactly(1));

    EXPECT_CALL(
        *mock_report,
        report_successful_display_construction()).Times(Exactly(1));

    EXPECT_CALL(
        *mock_report,
        report_egl_configuration(mock_egl.fake_egl_display,mock_egl.fake_configs[0])).Times(Exactly(1));

    auto display = std::make_shared<mgm::Display>(
                        create_platform(),
                        std::make_shared<mg::DefaultDisplayConfigurationPolicy>(),
                        std::make_shared<mtd::StubGLConfig>(),
                        mock_report);
}

TEST_F(MesaDisplayTest, outputs_correct_string_for_successful_setup_of_native_resources)
{
    using namespace ::testing;

    auto logger = std::make_shared<MockLogger>();
    auto reporter = std::make_shared<mrl::DisplayReport>(logger, std::make_shared<mtd::AdvanceableClock>());

    EXPECT_CALL(
        *logger,
        log(Eq(ml::Severity::informational),
            StrEq("Successfully setup native resources."),
            StrEq("graphics"))).Times(Exactly(1));

    reporter->report_successful_setup_of_native_resources();
}

TEST_F(MesaDisplayTest, outputs_correct_string_for_successful_egl_make_current_on_construction)
{
    using namespace ::testing;

    auto logger = std::make_shared<MockLogger>();
    auto reporter = std::make_shared<mrl::DisplayReport>(logger, std::make_shared<mtd::AdvanceableClock>());

    EXPECT_CALL(
        *logger,
        log(Eq(ml::Severity::informational),
            StrEq("Successfully made egl context current on construction."),
            StrEq("graphics"))).Times(Exactly(1));

    reporter->report_successful_egl_make_current_on_construction();
}

TEST_F(MesaDisplayTest, outputs_correct_string_for_successful_egl_buffer_swap_on_construction)
{
    using namespace ::testing;

    auto logger = std::make_shared<MockLogger>();
    auto reporter = std::make_shared<mrl::DisplayReport>(logger, std::make_shared<mtd::AdvanceableClock>());

    EXPECT_CALL(
        *logger,
        log(Eq(ml::Severity::informational),
            StrEq("Successfully performed egl buffer swap on construction."),
            StrEq("graphics"))).Times(Exactly(1));

    reporter->report_successful_egl_buffer_swap_on_construction();
}

TEST_F(MesaDisplayTest, outputs_correct_string_for_successful_drm_mode_set_crtc_on_construction)
{
    using namespace ::testing;

    auto logger = std::make_shared<MockLogger>();
    auto reporter = std::make_shared<mrl::DisplayReport>(logger, std::make_shared<mtd::AdvanceableClock>());

    EXPECT_CALL(
        *logger,
        log(Eq(ml::Severity::informational),
            StrEq("Successfully performed drm mode setup on construction."),
            StrEq("graphics"))).Times(Exactly(1));

    reporter->report_successful_drm_mode_set_crtc_on_construction();
}

// Disabled until mesa drm platform and mir platform properly shows support for those extensions
TEST_F(MesaDisplayTest, DISABLED_constructor_throws_if_egl_khr_image_pixmap_not_supported)
{
    using namespace ::testing;

    const char* egl_exts = "EGL_KHR_image EGL_KHR_image_base";

    EXPECT_CALL(mock_egl, eglQueryString(_,EGL_EXTENSIONS))
    .WillOnce(Return(egl_exts));

    EXPECT_THROW(
    {
        auto display = create_display(create_platform());
    }, std::runtime_error);
}

TEST_F(MesaDisplayTest, constructor_throws_if_gl_oes_image_not_supported)
{
    using namespace ::testing;

    const char* gl_exts = "GL_OES_texture_npot GL_OES_blend_func_separate";

    EXPECT_CALL(mock_gl, glGetString(GL_EXTENSIONS))
    .WillOnce(Return(reinterpret_cast<const GLubyte*>(gl_exts)));

    EXPECT_THROW(
    {
        auto display = create_display(create_platform());
    }, std::runtime_error);
}

TEST_F(MesaDisplayTest, for_each_display_buffer_calls_callback)
{
    using namespace ::testing;

    auto display = create_display(create_platform());

    int callback_count{0};

    display->for_each_display_sync_group([&](mg::DisplaySyncGroup& group) {
        group.for_each_display_buffer([&](mg::DisplayBuffer&) {
            callback_count++;
        });
    });

    EXPECT_NE(0, callback_count);
}

TEST_F(MesaDisplayTest, constructor_sets_vt_graphics_mode)
{
    using namespace testing;

    auto mock_vt = std::make_shared<mtd::MockVirtualTerminal>();

    EXPECT_CALL(*mock_vt, set_graphics_mode())
        .Times(1);

    auto platform = std::make_shared<mgm::Platform>(
        null_report,
        mock_vt,
        *std::make_shared<mtd::NullEmergencyCleanup>(),
        mgm::BypassOption::allowed);

    auto display = create_display(platform);
}

TEST_F(MesaDisplayTest, pause_drops_drm_master)
{
    using namespace testing;

    EXPECT_CALL(mock_drm, drmDropMaster(mock_drm.fake_drm.fd()))
        .Times(1);

    auto display = create_display(create_platform());

    display->pause();
}

TEST_F(MesaDisplayTest, resume_sets_drm_master)
{
    using namespace testing;

    EXPECT_CALL(mock_drm, drmSetMaster(mock_drm.fake_drm.fd()))
        .Times(1);

    auto display = create_display(create_platform());

    display->resume();
}

TEST_F(MesaDisplayTest, set_or_drop_drm_master_failure_throws_and_reports_error)
{
    using namespace testing;

    EXPECT_CALL(mock_drm, drmDropMaster(_))
        .WillOnce(SetErrnoAndReturn(EACCES, -EACCES));

    EXPECT_CALL(mock_drm, drmSetMaster(_))
        .WillOnce(SetErrnoAndReturn(EPERM, -EPERM));

    EXPECT_CALL(*mock_report, report_drm_master_failure(EACCES));
    EXPECT_CALL(*mock_report, report_drm_master_failure(EPERM));

    auto platform = std::make_shared<mgm::Platform>(
        mock_report,
        std::make_shared<mtd::NullVirtualTerminal>(),
        *std::make_shared<mtd::NullEmergencyCleanup>(),
        mgm::BypassOption::allowed);
    auto display = std::make_shared<mgm::Display>(
                        platform,
                        std::make_shared<mg::DefaultDisplayConfigurationPolicy>(),
                        std::make_shared<mtd::StubGLConfig>(),
                        mock_report);

    EXPECT_THROW({
        display->pause();
    }, std::runtime_error);

    EXPECT_THROW({
        display->resume();
    }, std::runtime_error);
}

TEST_F(MesaDisplayTest, configuration_change_registers_video_devices_handler)
{
    using namespace testing;

    auto display = create_display(create_platform());
    MockEventRegister mock_register;

    EXPECT_CALL(mock_register, register_fd_handler(_,_,_));

    display->register_configuration_change_handler(mock_register, []{});
}

TEST_F(MesaDisplayTest, drm_device_change_event_triggers_handler)
{
    using namespace testing;

    auto display = create_display(create_platform());

    mir::GLibMainLoop ml{std::make_shared<mir::time::SteadyClock>()};
    std::condition_variable done;

    int const device_add_count{1};
    int const device_change_count{10};
    int const expected_call_count{device_add_count + device_change_count};
    int call_count{0};
    std::mutex m;

    display->register_configuration_change_handler(
        ml,
        [&call_count, &ml, &done, &m]()
        {
            std::unique_lock<std::mutex> lock(m);
            if (++call_count == expected_call_count)
            {
                ml.stop();
                done.notify_all();
            }
        });

    std::thread t{
        [this]
        {
            auto const syspath = fake_devices.add_device("drm", "card2", NULL, {}, {"DEVTYPE", "drm_minor"});

            for (int i = 0; i != device_change_count; ++i)
            {
                // sleeping between calls to fake_devices hides race conditions
                std::this_thread::sleep_for(std::chrono::microseconds{500});
                fake_devices.emit_device_changed(syspath);
            }
        }};

    std::thread watchdog{
        [this, &done, &m, &ml, &call_count]
        {
            std::unique_lock<std::mutex> lock(m);
            if (!done.wait_for(lock, std::chrono::seconds{1}, [&call_count]() { return call_count == expected_call_count; }))
                ml.stop();
        }
    };

    ml.run();

    t.join();
    watchdog.join();

    EXPECT_EQ(expected_call_count, call_count);
}

TEST_F(MesaDisplayTest, respects_gl_config)
{
    using namespace testing;

    mtd::MockGLConfig mock_gl_config;
    EGLint const depth_bits{24};
    EGLint const stencil_bits{8};

    EXPECT_CALL(mock_gl_config, depth_buffer_bits())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(depth_bits));
    EXPECT_CALL(mock_gl_config, stencil_buffer_bits())
        .Times(AtLeast(1))
        .WillRepeatedly(Return(stencil_bits));

    EXPECT_CALL(mock_egl,
                eglChooseConfig(
                    _,
                    AllOf(mtd::EGLConfigContainsAttrib(EGL_DEPTH_SIZE, depth_bits),
                          mtd::EGLConfigContainsAttrib(EGL_STENCIL_SIZE, stencil_bits)),
                    _,_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                        SetArgPointee<4>(1),
                        Return(EGL_TRUE)));

    mgm::Display display{
        create_platform(),
        std::make_shared<mg::DefaultDisplayConfigurationPolicy>(),
        mir::test::fake_shared(mock_gl_config),
        null_report};
}

TEST_F(MesaDisplayTest, supports_as_low_as_15bit_colour)
{  // Regression test for LP: #1212753
    using namespace testing;

    mtd::StubGLConfig stub_gl_config;

    EXPECT_CALL(mock_egl,
                eglChooseConfig(
                    _,
                    AllOf(mtd::EGLConfigContainsAttrib(EGL_RED_SIZE, 5),
                          mtd::EGLConfigContainsAttrib(EGL_GREEN_SIZE, 5),
                          mtd::EGLConfigContainsAttrib(EGL_BLUE_SIZE, 5),
                          mtd::EGLConfigContainsAttrib(EGL_ALPHA_SIZE, 0)),
                    _,_,_))
        .Times(AtLeast(1))
        .WillRepeatedly(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                        SetArgPointee<4>(1),
                        Return(EGL_TRUE)));

    mgm::Display display{
        create_platform(),
        std::make_shared<mg::DefaultDisplayConfigurationPolicy>(),
        mir::test::fake_shared(stub_gl_config),
        null_report};
}
