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
#include "mir/graphics/drm_authenticator.h"
#include "src/server/graphics/gbm/gbm_platform.h"
#include "src/server/graphics/gbm/internal_client.h"
#include "mir_test_doubles/null_virtual_terminal.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_buffer_packer.h"

#include "mir/graphics/null_display_report.h"

#include <gtest/gtest.h>

#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdexcept>
#include <atomic>
#include <thread>
#include <chrono>

namespace mg = mir::graphics;
namespace mgg = mir::graphics::gbm;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;

namespace
{

class GBMGraphicsPlatform : public ::testing::Test
{
public:
    void SetUp()
    {
        ::testing::Mock::VerifyAndClearExpectations(&mock_drm);
        ::testing::Mock::VerifyAndClearExpectations(&mock_gbm);
    }

    std::shared_ptr<mg::Platform> create_platform()
    {
        return std::make_shared<mgg::GBMPlatform>(
            std::make_shared<mg::NullDisplayReport>(),
            std::make_shared<mtd::NullVirtualTerminal>());
    }

    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;
};
}

TEST_F(GBMGraphicsPlatform, get_ipc_package)
{
    using namespace testing;
    const int auth_fd{66};

    /* First time for master DRM fd, second for authenticated fd */
    EXPECT_CALL(mock_drm, drmOpen(_,_))
        .Times(2)
        .WillOnce(Return(mock_drm.fake_drm.fd()))
        .WillOnce(Return(auth_fd));

    /* Expect proper authorization */
    EXPECT_CALL(mock_drm, drmGetMagic(auth_fd,_));
    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),_));

    EXPECT_CALL(mock_drm, drmClose(mock_drm.fake_drm.fd()));

    /* Expect authenticated fd to be closed when package is destroyed */
    EXPECT_CALL(mock_drm, drmClose(auth_fd));

    EXPECT_NO_THROW (
        auto platform = create_platform();
        auto pkg = platform->get_ipc_package();

        ASSERT_TRUE(pkg.get());
        ASSERT_EQ(std::vector<int32_t>::size_type{1}, pkg->ipc_fds.size());
        ASSERT_EQ(auth_fd, pkg->ipc_fds[0]);
    );
}

TEST_F(GBMGraphicsPlatform, a_failure_while_creating_a_platform_results_in_an_error)
{
    using namespace ::testing;

    EXPECT_CALL(mock_drm, drmOpen(_,_))
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

/* ipc packaging tests */
TEST_F(GBMGraphicsPlatform, test_ipc_data_packed_correctly)
{
    auto mock_buffer = std::make_shared<mtd::MockBuffer>();
    mir::geometry::Stride dummy_stride(4390);

    auto native_handle = std::make_shared<MirBufferPackage>();
    native_handle->data_items = 4;
    native_handle->fd_items = 2;
    for(auto i=0; i<mir_buffer_package_max; i++)
    {
        native_handle->fd[i] = i; 
        native_handle->data[i] = i; 
    }

    EXPECT_CALL(*mock_buffer, native_buffer_handle())
        .WillOnce(testing::Return(native_handle));
    EXPECT_CALL(*mock_buffer, stride())
        .WillOnce(testing::Return(mir::geometry::Stride{dummy_stride}));

    auto platform = create_platform();

    auto mock_packer = std::make_shared<mtd::MockPacker>();
    for(auto i=0; i < native_handle->fd_items; i++)
    {
        EXPECT_CALL(*mock_packer, pack_fd(native_handle->fd[i]))
            .Times(1);
    } 
    for(auto i=0; i < native_handle->data_items; i++)
    {
        EXPECT_CALL(*mock_packer, pack_data(native_handle->data[i]))
            .Times(1);
    }
    EXPECT_CALL(*mock_packer, pack_stride(dummy_stride))
        .Times(1);

    platform->fill_ipc_package(mock_packer, mock_buffer);
}

TEST_F(GBMGraphicsPlatform, drm_auth_magic_calls_drm_function_correctly)
{
    using namespace testing;

    drm_magic_t const magic{0x10111213};

    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),magic))
        .WillOnce(Return(0));

    auto platform = create_platform();
    auto authenticator = std::dynamic_pointer_cast<mg::DRMAuthenticator>(platform);
    authenticator->drm_auth_magic(magic);
}

TEST_F(GBMGraphicsPlatform, drm_auth_magic_throws_if_drm_function_fails)
{
    using namespace testing;

    drm_magic_t const magic{0x10111213};

    EXPECT_CALL(mock_drm, drmAuthMagic(mock_drm.fake_drm.fd(),magic))
        .WillOnce(Return(-1));

    auto platform = create_platform();
    auto authenticator = std::dynamic_pointer_cast<mg::DRMAuthenticator>(platform);

    EXPECT_THROW({
        authenticator->drm_auth_magic(magic);
    }, std::runtime_error);
}

/* TODO: this function is a bit fragile because libmirserver and libmirclient both have very different
 *       implementations and both have symbols for it. If the linking order of the test changes,
 *       specifically, if mir_egl_mesa_display_is_valid resolves into libmirclient, then this test will break. 
 */
TEST_F(GBMGraphicsPlatform, platform_provides_validation_of_display_for_internal_clients)
{
    MirMesaEGLNativeDisplay* native_display = nullptr;
    EXPECT_EQ(0, mir_server_internal_display_is_valid(native_display));
    {
        auto platform = create_platform();
        auto client = platform->create_internal_client();
        native_display = reinterpret_cast<MirMesaEGLNativeDisplay*>(client->egl_native_display());
        EXPECT_EQ(1, mir_server_internal_display_is_valid(native_display));
    }
    EXPECT_EQ(0, mir_server_internal_display_is_valid(native_display));
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
TEST_F(GBMGraphicsPlatform, drm_close_not_called_concurrently_on_ipc_package_destruction)
{
    using namespace testing;

    unsigned int const num_threads{10};
    unsigned int const num_iterations{10};

    ConcurrentCallDetector detector;

    ON_CALL(mock_drm, drmClose(_))
        .WillByDefault(DoAll(InvokeWithoutArgs(&detector, &ConcurrentCallDetector::call),
                             Return(0)));

    auto platform = create_platform();

    std::vector<std::thread> threads;

    for (unsigned int i = 0; i < num_threads; i++)
    {
        threads.push_back(std::thread{
            [platform]
            {
                for (unsigned int i = 0; i < num_iterations; i++)
                {
                    platform->get_ipc_package();
                }
            }});
    }

    for (auto& t : threads)
        t.join();

    EXPECT_FALSE(detector.detected_concurrent_calls());
}

TEST_F(GBMGraphicsPlatform, supported_pixel_formats_contain_common_formats)
{
    auto platform = create_platform();
    auto supported_pixel_formats = platform->supported_pixel_formats();

    auto argb_8888_count = std::count(supported_pixel_formats.begin(),
                                      supported_pixel_formats.end(),
                                      geom::PixelFormat::argb_8888);

    auto xrgb_8888_count = std::count(supported_pixel_formats.begin(),
                                      supported_pixel_formats.end(),
                                      geom::PixelFormat::xrgb_8888);

    EXPECT_EQ(1, argb_8888_count);
    EXPECT_EQ(1, xrgb_8888_count);
}

TEST_F(GBMGraphicsPlatform, supported_pixel_formats_have_sane_default_in_first_position)
{
    auto platform = create_platform();
    auto supported_pixel_formats = platform->supported_pixel_formats();

    ASSERT_FALSE(supported_pixel_formats.empty());
    EXPECT_EQ(geom::PixelFormat::argb_8888, supported_pixel_formats[0]);
}
