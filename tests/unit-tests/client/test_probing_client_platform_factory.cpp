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

#include "src/client/client_platform.h"
#include "src/client/probing_client_platform_factory.h"

#include "mir_test_doubles/mock_client_context.h"
#include "mir_test_framework/executable_path.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;

TEST(ProbingClientPlatformFactory, ThrowsErrorWhenConstructedWithNoPlatforms)
{
    std::vector<std::shared_ptr<mir::SharedLibrary>> empty_modules;
    EXPECT_THROW(mir::client::ProbingClientPlatformFactory{empty_modules},
                 std::runtime_error);
}

TEST(ProbingClientPlatformFactory, ThrowsErrorWhenNoPlatformPluginProbesSuccessfully)
{
    using namespace testing;

    auto android_platform = std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/client-platform-android.so");
    auto mesa_platform = std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/client-platform-mesa.so");

    std::vector<std::shared_ptr<mir::SharedLibrary>> modules = {android_platform, mesa_platform};
    mir::client::ProbingClientPlatformFactory factory{modules};

    mtd::MockClientContext context;
    ON_CALL(context, populate(_))
            .WillByDefault(Invoke([](MirPlatformPackage& pkg)
                           {
                               ::memset(&pkg, 0, sizeof(MirPlatformPackage));
                               // Mock up a platform package that looks nothing like
                               // either an Android or Mesa package
                               pkg.fd_items = 0xdeadbeef;
                               pkg.data_items = -23;
                           }));

    EXPECT_THROW(factory.create_client_platform(&context),
                 std::runtime_error);
}

TEST(ProbingClientPlatformFactory, CreatesPlatformFromAppropriateModule)
{
    using namespace testing;

    auto android_platform = std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/client-platform-android.so");
    auto mesa_platform = std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/client-platform-mesa.so");

    std::vector<std::shared_ptr<mir::SharedLibrary>> modules = {android_platform, mesa_platform};
    mir::client::ProbingClientPlatformFactory factory{modules};

    mtd::MockClientContext context;
    ON_CALL(context, populate(_))
            .WillByDefault(Invoke([](MirPlatformPackage& pkg)
                           {
                               ::memset(&pkg, 0, sizeof(MirPlatformPackage));
                               // Mock up something that looks like a GBM platform package,
                               // until we send the actual platform type over the wire!
                               pkg.fd_items = 1;
                               pkg.fd[0] = 23;
                           }));
    auto platform = factory.create_client_platform(&context);
    EXPECT_EQ(mir_platform_type_gbm, platform->platform_type());

    ON_CALL(context, populate(_))
            .WillByDefault(Invoke([](MirPlatformPackage& pkg)
                           {
                               // Mock up something that looks like a GBM platform package,
                               // until we send the actual platform type over the wire!
                               ::memset(&pkg, 0, sizeof(MirPlatformPackage));
                           }));

    platform = factory.create_client_platform(&context);
    EXPECT_EQ(mir_platform_type_android, platform->platform_type());
}
