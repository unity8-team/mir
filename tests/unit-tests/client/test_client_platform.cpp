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

#include "src/client/client_platform.h"
#include "src/client/mir_client_surface.h"
#include "mir_test_doubles/mock_client_context.h"
#include "mir_test_doubles/mock_client_surface.h"
#include "mir_test_framework/executable_path.h"

#include "mir_test_doubles/mock_android_hw.h"
#include "src/client/client_platform_factory.h"

#include "mir/shared_library.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mcl=mir::client;
namespace mtd = mir::test::doubles;
namespace mtf = mir_test_framework;

namespace
{
struct ClientPlatformTraits
{
    ClientPlatformTraits(std::string const& library,
                         std::function<void(MirPlatformPackage&)> populator,
                         MirPlatformType type)
        : platform_library_name{library},
          populate_package_for{populator},
          platform_type{type}
    {
    }

    std::string const platform_library_name;
    std::function<void(MirPlatformPackage&)> const populate_package_for;
    MirPlatformType platform_type;
};

struct ClientPlatformTest : public ::testing::TestWithParam<ClientPlatformTraits>
{
    ClientPlatformTest()
        : platform_library{mtf::library_path() + "/" + GetParam().platform_library_name},
          create_client_platform{platform_library.load_function<mcl::CreateClientPlatform>("create_client_platform")}
    {
        using namespace testing;
        ON_CALL(context, populate(_))
            .WillByDefault(Invoke(GetParam().populate_package_for));
    }

    mtd::MockClientContext context;
#ifdef MIR_BUILD_PLATFORM_ANDROID
    testing::NiceMock<mtd::HardwareAccessMock> hw_access_mock;
#endif
    mir::SharedLibrary platform_library;
    mcl::CreateClientPlatform const create_client_platform;
};

#ifdef MIR_BUILD_PLATFORM_ANDROID
ClientPlatformTraits const android_platform{"client-platform-android.so",
                                            [](MirPlatformPackage& pkg)
                                            {
                                                ::memset(&pkg, 0, sizeof(pkg));
                                            },
                                            mir_platform_type_android
                                           };
#else
ClientPlatformTraits const android_platform{"client-platform-dummy.so",
                                            [](MirPlatformPackage&) {},
                                            mir_platform_type_gbm
                                           };
#endif

#ifdef MIR_BUILD_PLATFORM_MESA
ClientPlatformTraits const mesa_platform{"client-platform-mesa.so",
                                         [](MirPlatformPackage& pkg)
                                         {
                                             ::memset(&pkg, 0, sizeof(pkg));
                                             pkg.fd_items = 1;
                                         },
                                         mir_platform_type_gbm
                                        };
#else
ClientPlatformTraits const mesa_platform{"client-platform-dummy.so",
                                         [](MirPlatformPackage&) {},
                                         mir_platform_type_gbm
                                        };
#endif
}

INSTANTIATE_TEST_CASE_P(PlatformLibraries,
                        ClientPlatformTest,
                        ::testing::Values(android_platform, mesa_platform));

TEST_P(ClientPlatformTest, platform_name)
{
    auto platform = create_client_platform(&context);

    EXPECT_EQ(GetParam().platform_type, platform->platform_type());
}

TEST_P(ClientPlatformTest, platform_creates)
{
    auto platform = create_client_platform(&context);
    auto buffer_factory = platform->create_buffer_factory();
    EXPECT_NE(buffer_factory.get(), (mcl::ClientBufferFactory*) NULL);
}

TEST_P(ClientPlatformTest, platform_creates_native_window)
{
    auto platform = create_client_platform(&context);
    auto mock_client_surface = std::make_shared<mtd::MockClientSurface>();
    auto native_window = platform->create_egl_native_window(mock_client_surface.get());
    EXPECT_NE(*native_window, (EGLNativeWindowType) NULL);
}

TEST_P(ClientPlatformTest, platform_creates_egl_native_display)
{
    auto platform = create_client_platform(&context);
    auto native_display = platform->create_egl_native_display();
    EXPECT_NE(nullptr, native_display.get());
}
