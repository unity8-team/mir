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

#include "mir/graphics/platform_ipc_package.h"
#include "mir/graphics/event_handler_register.h"
#include "mir/graphics/platform_ipc_operations.h"
#include "mir/graphics/platform_operation_message.h"
#include "src/platforms/mesa/server/platform.h"
#include "src/server/report/null_report_factory.h"
#include "mir/emergency_cleanup_registry.h"
#include "mir/shared_library.h"

#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_buffer_ipc_message.h"
#include "mir_test_doubles/platform_factory.h"
#include "mir_test_doubles/mock_virtual_terminal.h"
#include "mir_test_doubles/null_virtual_terminal.h"

#include <gtest/gtest.h>

#include "mir_test_framework/udev_environment.h"
#include "mir_test_framework/executable_path.h"
#include "mir_test/pipe.h"

#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"
#include "mir_test_doubles/fd_matcher.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{

const char probe_platform[] = "probe_graphics_platform";

class MesaGraphicsPlatform : public ::testing::Test
{
public:
    void SetUp()
    {
        ::testing::Mock::VerifyAndClearExpectations(&mock_drm);
        ::testing::Mock::VerifyAndClearExpectations(&mock_gbm);
        fake_devices.add_standard_device("standard-drm-devices");
    }

    std::shared_ptr<mg::Platform> create_platform()
    {
        return mtd::create_platform_with_null_dependencies();
    }

    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;
    mtf::UdevEnvironment fake_devices;
};
}

TEST_F(MesaGraphicsPlatform, connection_ipc_package)
{
    using namespace testing;
    mir::test::Pipe auth_pipe;
    int const auth_fd{auth_pipe.read_fd()};

    /* First time for master DRM fd, second for authenticated fd */
    EXPECT_CALL(mock_drm, open(_,_,_))
        .WillOnce(Return(mock_drm.fake_drm.fd()));
    EXPECT_CALL(mock_drm, drmOpen(_,_))
        .WillOnce(Return(auth_fd));

    /* Expect proper authorization */
    EXPECT_CALL(mock_drm, drmGetMagic(auth_fd,_));
    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),_));

    EXPECT_CALL(mock_drm, drmClose(mock_drm.fake_drm.fd()));

    /* Expect authenticated fd to be closed when package is destroyed */
    EXPECT_CALL(mock_drm, drmClose(auth_fd));

    EXPECT_NO_THROW (
        auto platform = create_platform();
        auto ipc_ops = platform->make_ipc_operations();
        auto pkg = ipc_ops->connection_ipc_package();

        ASSERT_TRUE(pkg.get());
        ASSERT_EQ(std::vector<int32_t>::size_type{1}, pkg->ipc_fds.size());
        ASSERT_EQ(auth_fd, pkg->ipc_fds[0]);
    );
}

TEST_F(MesaGraphicsPlatform, a_failure_while_creating_a_platform_results_in_an_error)
{
    using namespace ::testing;

    EXPECT_CALL(mock_drm, open(_,_,_))
            .WillRepeatedly(Return(-1));

    try
    {
        auto platform = create_platform();
    } catch(...)
    {
        return;
    }

    FAIL() << "Expected an exception to be thrown.";
}

TEST_F(MesaGraphicsPlatform, fails_if_no_resources)
{
    using namespace ::testing;

    EXPECT_CALL(mock_drm, drmModeGetResources(_))
        .Times(Exactly(1))
        .WillOnce(Return(reinterpret_cast<drmModeRes*>(0)));

    EXPECT_CALL(mock_drm, drmModeFreeResources(_))
        .Times(Exactly(0));

    EXPECT_THROW({
        auto platform = create_platform();
    }, std::runtime_error) << "Expected that c'tor of Platform throws";
}

TEST_F(MesaGraphicsPlatform, egl_native_display_is_gbm_device)
{
    auto platform = create_platform();
    EXPECT_EQ(mock_gbm.fake_gbm.device, platform->egl_native_display());
}

namespace
{

class ConcurrentCallDetector
{
public:
    ConcurrentCallDetector()
        : threads_in_call{0}, detected_concurrent_calls_{false}
    {
    }

    void call()
    {
        if (threads_in_call.fetch_add(1) > 0)
            detected_concurrent_calls_ = true;

        std::this_thread::sleep_for(std::chrono::milliseconds{1});

        --threads_in_call;
    }

    bool detected_concurrent_calls()
    {
        return detected_concurrent_calls_;
    }

private:
    std::atomic<int> threads_in_call;
    std::atomic<bool> detected_concurrent_calls_;
};

}

/*
 * This test is not 100% reliable in theory (we are trying to recreate a race
 * condition after all!), but it can only produce false successes, not false
 * failures, so it's safe to use.  In practice it is reliable enough: I get a
 * 100% failure rate for this test (1000 out of 1000 repetitions) when testing
 * without the fix for the race condition we are testing for.
 */
TEST_F(MesaGraphicsPlatform, drm_close_not_called_concurrently_on_ipc_package_destruction)
{
    using namespace testing;

    unsigned int const num_threads{10};
    unsigned int const num_iterations{10};

    ConcurrentCallDetector detector;

    ON_CALL(mock_drm, drmClose(_))
        .WillByDefault(DoAll(InvokeWithoutArgs(&detector, &ConcurrentCallDetector::call),
                             Return(0)));

    auto platform = create_platform();
    auto ipc_ops = platform->make_ipc_operations();

    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < num_threads; i++)
    {
        threads.push_back(std::thread{
            [ipc_ops]
            {
                for (unsigned int i = 0; i < num_iterations; i++)
                {
                    ipc_ops->connection_ipc_package();
                }
            }});
    }

    for (auto& t : threads)
        t.join();

    EXPECT_FALSE(detector.detected_concurrent_calls());
}

struct StubEmergencyCleanupRegistry : mir::EmergencyCleanupRegistry
{
    void add(mir::EmergencyCleanupHandler const& handler) override
    {
        this->handler = handler;
    }

    mir::EmergencyCleanupHandler handler;
};

TEST_F(MesaGraphicsPlatform, restores_vt_on_emergency_cleanup)
{
    using namespace testing;

    auto const mock_vt = std::make_shared<mtd::MockVirtualTerminal>();
    StubEmergencyCleanupRegistry emergency_cleanup_registry;
    mgm::Platform platform{
        mir::report::null_display_report(),
        mock_vt,
        emergency_cleanup_registry,
        mgm::BypassOption::allowed};

    EXPECT_CALL(*mock_vt, restore());

    emergency_cleanup_registry.handler();

    Mock::VerifyAndClearExpectations(mock_vt.get());
}

TEST_F(MesaGraphicsPlatform, releases_drm_on_emergency_cleanup)
{
    using namespace testing;

    auto const null_vt = std::make_shared<mtd::NullVirtualTerminal>();
    StubEmergencyCleanupRegistry emergency_cleanup_registry;
    mgm::Platform platform{
        mir::report::null_display_report(),
        null_vt,
        emergency_cleanup_registry,
        mgm::BypassOption::allowed};

    int const success_code = 0;
    EXPECT_CALL(mock_drm, drmDropMaster(mock_drm.fake_drm.fd()))
        .WillOnce(Return(success_code));

    emergency_cleanup_registry.handler();

    Mock::VerifyAndClearExpectations(&mock_drm);
}

TEST_F(MesaGraphicsPlatform, does_not_propagate_emergency_cleanup_exceptions)
{
    using namespace testing;

    auto const mock_vt = std::make_shared<mtd::MockVirtualTerminal>();
    StubEmergencyCleanupRegistry emergency_cleanup_registry;
    mgm::Platform platform{
        mir::report::null_display_report(),
        mock_vt,
        emergency_cleanup_registry,
        mgm::BypassOption::allowed};

    EXPECT_CALL(*mock_vt, restore())
        .WillOnce(Throw(std::runtime_error("vt restore exception")));
    EXPECT_CALL(mock_drm, drmDropMaster(mock_drm.fake_drm.fd()))
        .WillOnce(Throw(std::runtime_error("drm drop master exception")));

    emergency_cleanup_registry.handler();

    Mock::VerifyAndClearExpectations(&mock_drm);
}

TEST_F(MesaGraphicsPlatform, probe_returns_unsupported_when_no_drm_udev_devices)
{
    mtf::UdevEnvironment udev_environment;

    mir::SharedLibrary platform_lib{mtf::server_platform("graphics-mesa")};
    auto probe = platform_lib.load_function<mg::PlatformProbe>(probe_platform);
    EXPECT_EQ(mg::PlatformPriority::unsupported, probe());
}

TEST_F(MesaGraphicsPlatform, probe_returns_best_when_drm_devices_exist)
{
    mtf::UdevEnvironment udev_environment;

    udev_environment.add_standard_device("standard-drm-devices");

    mir::SharedLibrary platform_lib{mtf::server_platform("graphics-mesa")};
    auto probe = platform_lib.load_function<mg::PlatformProbe>(probe_platform);
    EXPECT_EQ(mg::PlatformPriority::best, probe());
}
