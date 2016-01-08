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

#include "mir/client_platform.h"
#include "src/client/probing_client_platform_factory.h"
#include "src/server/report/null_report_factory.h"

#include "mir/test/doubles/mock_client_context.h"
#include "mir_test_framework/executable_path.h"
#include "mir_test_framework/stub_platform_helpers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <dlfcn.h>

namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;

namespace
{
std::vector<std::string>
all_available_modules()
{
    std::vector<std::string> modules;
#if defined(MIR_BUILD_PLATFORM_MESA_KMS) || defined(MIR_BUILD_PLATFORM_MESA_X11)
    modules.push_back(mtf::client_platform("mesa"));
#endif
#ifdef MIR_BUILD_PLATFORM_ANDROID
    modules.push_back(mtf::client_platform("android"));
#endif
    return modules;
}

bool loaded(std::string const& path)
{
    void* x = dlopen(path.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    if (x)
        dlclose(x);
    return !!x;
}

void populate_valid(MirPlatformPackage& pkg)
{
    memset(&pkg, 0, sizeof(MirPlatformPackage));
    pkg.fd_items = 1;
    pkg.fd[0] = 23;
}

void safely_unload(std::shared_ptr<mir::client::ClientPlatform>& platform)
{
    ASSERT_TRUE(platform.unique());
    auto library = platform->keep_library_loaded();
    platform.reset();
    library.reset();
}

}

TEST(ProbingClientPlatformFactory, ThrowsErrorWhenConstructedWithNoPlatforms)
{
    std::vector<std::shared_ptr<mir::SharedLibrary>> empty_modules;
    EXPECT_THROW(mir::client::ProbingClientPlatformFactory(
                     mir::report::null_shared_library_prober_report(),
                     {}, {}),
                 std::runtime_error);
}

TEST(ProbingClientPlatformFactory, ThrowsErrorWhenNoPlatformPluginProbesSuccessfully)
{
    using namespace testing;

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        all_available_modules(),
        {});

    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
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

TEST(ProbingClientPlatformFactory, DoesNotLeakTheUsedDriverModuleOnShutdown)
{   // Regression test for LP: #1527449
    using namespace testing;
    auto const modules = all_available_modules();
    ASSERT_FALSE(modules.empty());
    std::string const preferred_module = modules.front();

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        {preferred_module},
        {});

    std::shared_ptr<mir::client::ClientPlatform> platform;
    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
            .WillByDefault(Invoke(populate_valid));

    ASSERT_FALSE(loaded(preferred_module));
    platform = factory.create_client_platform(&context);
    ASSERT_TRUE(loaded(preferred_module));
    safely_unload(platform);
    EXPECT_FALSE(loaded(preferred_module));
}

TEST(ProbingClientPlatformFactory, DoesNotLeakUnusedDriverModulesOnStartup)
{   // Regression test for LP: #1527449 and LP: #1526658
    using namespace testing;
    auto const modules = all_available_modules();
    ASSERT_FALSE(modules.empty());

    // Note: This test is only really effective with nmodules>1, which many of
    //       our builds will have. But nmodules==1 is harmless.

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        modules,
        {});

    std::shared_ptr<mir::client::ClientPlatform> platform;
    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
            .WillByDefault(Invoke(populate_valid));

    int nloaded = 0;
    for (auto const& m : modules)
        if (loaded(m)) ++nloaded;
    ASSERT_EQ(0, nloaded);

    platform = factory.create_client_platform(&context);

    nloaded = 0;
    for (auto const& m : modules)
        if (loaded(m)) ++nloaded;
    EXPECT_EQ(1, nloaded);  // expect not assert, because we need safely_unload

    safely_unload(platform);

    nloaded = 0;
    for (auto const& m : modules)
        if (loaded(m)) ++nloaded;
    ASSERT_EQ(0, nloaded);
}

// Note "DeathTest" informs our scripts that we expect a leak from the
// child process that died, and to ignore it.
TEST(ProbingClientPlatformFactoryDeathTest, DiesOnUnsafeRelease)
{
    using namespace testing;
    auto const modules = all_available_modules();
    ASSERT_FALSE(modules.empty());
    std::string const preferred_module = modules.front();

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        {preferred_module},
        {});

    std::shared_ptr<mir::client::ClientPlatform> platform;
    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
            .WillByDefault(Invoke(populate_valid));

    platform = factory.create_client_platform(&context);

    // Google Test creates a child process to verify this:
    ASSERT_DEATH({platform.reset();}, ".*still in use.*");

    // ... but here in the parent process we need to actively avoid the
    // death happening, so that our test completes:
    safely_unload(platform);
}

#if defined(MIR_BUILD_PLATFORM_MESA_KMS) || defined(MIR_BUILD_PLATFORM_MESA_X11)
TEST(ProbingClientPlatformFactory, CreatesMesaPlatformWhenAppropriate)
#else
TEST(ProbingClientPlatformFactory, DISABLED_CreatesMesaPlatformWhenAppropriate)
#endif
{
    using namespace testing;

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        all_available_modules(),
        {});

    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
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
    safely_unload(platform);
}

#ifdef MIR_BUILD_PLATFORM_ANDROID
TEST(ProbingClientPlatformFactory, CreatesAndroidPlatformWhenAppropriate)
#else
TEST(ProbingClientPlatformFactory, DISABLED_CreatesAndroidPlatformWhenAppropriate)
#endif
{
    using namespace testing;

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        all_available_modules(),
        {});

    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
            .WillByDefault(Invoke([](MirPlatformPackage& pkg)
                           {
                               // Mock up something that looks like a Android platform package,
                               // until we send the actual platform type over the wire!
                               ::memset(&pkg, 0, sizeof(MirPlatformPackage));
                           }));

    auto platform = factory.create_client_platform(&context);
    EXPECT_EQ(mir_platform_type_android, platform->platform_type());
    safely_unload(platform);
}

TEST(ProbingClientPlatformFactory, IgnoresNonClientPlatformModules)
{
    using namespace testing;

    auto modules = all_available_modules();
    // NOTE: For minimum fuss, load something that has minimal side-effects...
    modules.push_back("libc.so.6");
    modules.push_back(mtf::client_platform("dummy.so"));

    mir::client::ProbingClientPlatformFactory factory(
        mir::report::null_shared_library_prober_report(),
        modules,
        {});

    mtd::MockClientContext context;
    ON_CALL(context, populate_server_package(_))
            .WillByDefault(Invoke([](MirPlatformPackage& pkg)
                           {
                               mtf::create_stub_platform_package(pkg);
                           }));

    auto platform = factory.create_client_platform(&context);
    safely_unload(platform);
}
