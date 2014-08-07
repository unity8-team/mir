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

#include <gtest/gtest.h>

#include "src/server/graphics/platform_probe.h"
#include "mir/graphics/platform.h"

#include "mir/raii.h"

#ifdef MIR_BUILD_PLATFORM_MESA
#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"
#endif

#include "mir_test_doubles/mock_android_hw.h"
#include "mir_test_framework/udev_environment.h"
#include "mir_test_framework/executable_path.h"

namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;

namespace
{
std::vector<std::shared_ptr<mir::SharedLibrary>> available_platforms()
{
    std::vector<std::shared_ptr<mir::SharedLibrary>> modules;

#ifdef MIR_BUILD_PLATFORM_MESA
    modules.push_back(std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/platform-graphics-mesa.so"));
#endif
#ifdef MIR_BUILD_PLATFORM_ANDROID
    modules.push_back(std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/platform-graphics-android.so"));
#endif
    return modules;
}

void add_dummy_platform(std::vector<std::shared_ptr<mir::SharedLibrary>>& modules)
{
    modules.insert(modules.begin(), std::make_shared<mir::SharedLibrary>(mtf::library_path() + "/platform-graphics-dummy.so"));
}

std::shared_ptr<void> ensure_android_probing_fails()
{
#ifdef MIR_BUILD_PLATFORM_ANDROID
    using namespace testing;
    auto mock_android = std::make_shared<NiceMock<mtd::HardwareAccessMock>>();
    ON_CALL(*mock_android, hw_get_module(_, _))
       .WillByDefault(Return(-1));
    return mock_android;
#else
    return std::shared_ptr<void>{};
#endif
}

std::shared_ptr<void> ensure_mesa_probing_fails()
{
    return std::make_shared<mtf::UdevEnvironment>();
}

std::shared_ptr<void> ensure_mesa_probing_succeeds()
{
    auto udev = std::make_shared<mtf::UdevEnvironment>();

    udev->add_standard_device("standard-drm-devices");

    return udev;
}

std::shared_ptr<void> ensure_android_probing_succeeds()
{
#ifdef MIR_BUILD_PLATFORM_ANDROID
    using namespace testing;
    auto mock_android = std::make_shared<NiceMock<mtd::HardwareAccessMock>>();
    ON_CALL(*mock_android, hw_get_module(_, _))
       .WillByDefault(Return(0));
    return mock_android;
#else
    return std::shared_ptr<void>{};
#endif
}
}

TEST(ServerPlatformProbe, ConstructingWithNoModulesIsAnError)
{
    std::vector<std::shared_ptr<mir::SharedLibrary>> empty_modules;
    EXPECT_THROW(mir::graphics::module_for_device(empty_modules),
                 std::runtime_error);
}

#ifdef MIR_BUILD_PLATFORM_MESA
TEST(ServerPlatformProbe, LoadsMesaPlatformWhenDrmDevicePresent)
{
    using namespace testing;
    auto block_android = ensure_android_probing_fails();
    auto fake_mesa = ensure_mesa_probing_succeeds();

    auto modules = available_platforms();

    auto module = mir::graphics::module_for_device(modules);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, HasSubstr("mesa"));
}
#endif

#ifdef MIR_BUILD_PLATFORM_ANDROID
TEST(ServerPlatformProbe, LoadsAndroidPlatformWhenHwaccessSucceeds)
{
    using namespace testing;

    auto block_mesa = ensure_mesa_probing_fails();
    auto fake_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();

    auto module = mir::graphics::module_for_device(modules);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, HasSubstr("android"));
}
#endif

TEST(ServerPlatformProbe, ThrowsExceptionWhenNothingProbesSuccessfully)
{
    using namespace testing;
    auto block_android = ensure_android_probing_fails();
    auto block_mesa = ensure_mesa_probing_fails();


    EXPECT_THROW(mir::graphics::module_for_device(available_platforms()),
                 std::runtime_error);
}

TEST(ServerPlatformProbe, LoadsSupportedModuleWhenNoBestModule)
{
    using namespace testing;
    auto block_android = ensure_android_probing_fails();
    auto block_mesa = ensure_mesa_probing_fails();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    auto module = mir::graphics::module_for_device(modules);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, HasSubstr("dummy"));
}

TEST(ServerPlatformProbe, LoadsMesaOrAndroidInPreferenceToDummy)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    auto module = mir::graphics::module_for_device(modules);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, Not(HasSubstr("dummy")));
}
