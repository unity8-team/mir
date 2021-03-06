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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/graphics/platform_authentication.h"
#include "src/platforms/mesa/server/kms/guest_platform.h"
#include "mir/graphics/buffer_properties.h"
#include "mir/graphics/platform_ipc_operations.h"
#include "mir_toolkit/mesa/platform_operation.h"
#include "mir_toolkit/extensions/set_gbm_device.h"

#include "mir/test/fake_shared.h"
#include "mir/test/doubles/mock_drm.h"
#include "mir/test/doubles/mock_gbm.h"
#include "mir/test/doubles/stub_buffer.h"
#include "mir/test/doubles/mock_buffer_ipc_message.h"
#include "mir/test/doubles/fd_matcher.h"
#include "mir/test/doubles/mock_platform_authentication.h"
#include "mir/test/doubles/mock_mesa_auth_extensions.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <cstring>
#include <fcntl.h>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace geom = mir::geometry;
using namespace testing;

namespace
{

struct MockSetGbmExt : mg::SetGbmExtension
{
    MOCK_METHOD1(set_gbm_device, void(gbm_device* dev));
};

class MesaGuestPlatformTest : public ::testing::Test
{
public:
    MesaGuestPlatformTest()
        : drm_fd{open(drm_device, 0, 0)}
    {
        int fake_fd = 4939;
        ON_CALL(mock_platform_authentication, platform_fd_items())
            .WillByDefault(Return(std::vector<int>{drm_fd}));
        ON_CALL(mock_platform_authentication, set_gbm_extension())
            .WillByDefault(Return(mir::optional_value<std::shared_ptr<mg::SetGbmExtension>>{mock_gbm_ext}));
        ON_CALL(mock_platform_authentication, auth_extension())
            .WillByDefault(Return(mir::optional_value<std::shared_ptr<mg::MesaAuthExtension>>{mock_ext}));
        ON_CALL(*mock_ext, auth_fd())
            .WillByDefault(Return(mir::Fd{mir::IntOwnedFd{fake_fd}}));
    }

protected:
    ::testing::NiceMock<mtd::MockDRM> mock_drm;
    ::testing::NiceMock<mtd::MockGBM> mock_gbm;
    ::testing::NiceMock<mtd::MockPlatformAuthentication> mock_platform_authentication;
    std::shared_ptr<mtd::MockMesaExt> mock_ext = std::make_shared<mtd::MockMesaExt>();
    std::shared_ptr<MockSetGbmExt> mock_gbm_ext = std::make_shared<MockSetGbmExt>();
    char const* const drm_device = "/dev/dri/card0";
    int const drm_fd;
};

}

TEST_F(MesaGuestPlatformTest, auth_fd_is_delegated_to_platform_authentication)
{
    int const auth_fd{13};

    EXPECT_CALL(mock_platform_authentication, set_gbm_extension());
    EXPECT_CALL(mock_platform_authentication, auth_extension())
        .Times(2);
    EXPECT_CALL(*mock_ext, auth_fd())
        .Times(2)
        .WillRepeatedly(Return(mir::Fd{mir::IntOwnedFd{auth_fd}}));

    mgm::GuestPlatform native(mt::fake_shared(mock_platform_authentication));
    auto ipc_ops = native.make_ipc_operations();
    ipc_ops->connection_ipc_package();
}

TEST_F(MesaGuestPlatformTest, sets_gbm_device_during_initialization)
{
    EXPECT_CALL(mock_platform_authentication, set_gbm_extension());
    EXPECT_CALL(*mock_gbm_ext, set_gbm_device(mock_gbm.fake_gbm.device));
    mgm::GuestPlatform native(mt::fake_shared(mock_platform_authentication));
}
