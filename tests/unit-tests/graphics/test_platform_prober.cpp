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
#include <gmock/gmock.h>

#include "src/platform/graphics/platform_probe_report.h"
#include "src/platform/graphics/platform_probe.h"
#include "mir/graphics/platform.h"

#include "mir/raii.h"

#ifdef MIR_BUILD_PLATFORM_MESA
#include "mir_test_doubles/mock_drm.h"
#include "mir_test_doubles/mock_gbm.h"
#endif

#ifdef MIR_BUILD_PLATFORM_ANDROID
#include "mir_test_doubles/mock_android_hw.h"
#endif

#include "mir_test_framework/udev_environment.h"
#include "mir_test_framework/executable_path.h"

namespace mtf = mir_test_framework;
namespace mtd = mir::test::doubles;
namespace mg = mir::graphics;

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

class MockPlatformProbeReport : public mir::graphics::PlatformProbeReport
{
public:
    MOCK_METHOD2(module_probed, void(mg::ModuleProperties const&, mg::PlatformPriority));
    MOCK_METHOD1(module_selected, void(mg::ModuleProperties const&));
    MOCK_METHOD1(invalid_module_probed, void(std::exception const&));
};
using StubPlatformProbeReport = testing::NiceMock<MockPlatformProbeReport>;

class ServerPlatformProbe : public testing::Test
{
public:
    StubPlatformProbeReport stub_probe_report;
};
}

TEST_F(ServerPlatformProbe, ConstructingWithNoModulesIsAnError)
{
    std::vector<std::shared_ptr<mir::SharedLibrary>> empty_modules;
    EXPECT_THROW(mir::graphics::module_for_device(empty_modules, stub_probe_report),
                 std::runtime_error);
}

#ifdef MIR_BUILD_PLATFORM_MESA
TEST_F(ServerPlatformProbe, LoadsMesaPlatformWhenDrmDevicePresent)
{
    using namespace testing;
    auto block_android = ensure_android_probing_fails();
    auto fake_mesa = ensure_mesa_probing_succeeds();

    auto modules = available_platforms();

    auto module = mir::graphics::module_for_device(modules, stub_probe_report);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, HasSubstr("mesa"));
}
#endif

#ifdef MIR_BUILD_PLATFORM_ANDROID
TEST_F(ServerPlatformProbe, LoadsAndroidPlatformWhenHwaccessSucceeds)
{
    using namespace testing;

    auto block_mesa = ensure_mesa_probing_fails();
    auto fake_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();

    auto module = mir::graphics::module_for_device(modules, stub_probe_report);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, HasSubstr("android"));
}
#endif

TEST_F(ServerPlatformProbe, ThrowsExceptionWhenNothingProbesSuccessfully)
{
    using namespace testing;
    auto block_android = ensure_android_probing_fails();
    auto block_mesa = ensure_mesa_probing_fails();


    EXPECT_THROW(mir::graphics::module_for_device(available_platforms(), stub_probe_report),
                 std::runtime_error);
}

TEST_F(ServerPlatformProbe, LoadsSupportedModuleWhenNoBestModule)
{
    using namespace testing;
    auto block_android = ensure_android_probing_fails();
    auto block_mesa = ensure_mesa_probing_fails();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    auto module = mir::graphics::module_for_device(modules, stub_probe_report);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, HasSubstr("dummy"));
}

TEST_F(ServerPlatformProbe, LoadsMesaOrAndroidInPreferenceToDummy)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    auto module = mir::graphics::module_for_device(modules, stub_probe_report);
    ASSERT_NE(nullptr, module);

    auto descriptor = module->load_function<mir::graphics::DescribeModule>("describe_module");
    auto description = descriptor();

    EXPECT_THAT(description->name, Not(HasSubstr("dummy")));
}

TEST_F(ServerPlatformProbe, LogsEachModuleProbed)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    NiceMock<MockPlatformProbeReport> report;
    EXPECT_CALL(report, module_probed(_,_)).Times(modules.size());

    auto module = mir::graphics::module_for_device(modules, report);
}

namespace
{
MATCHER_P(ModuleNameMatches, name, "")
{
    return strcmp(name, arg.name) == 0;
}
}

TEST_F(ServerPlatformProbe, LogsWithCorrectPriorityAllSupported)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    NiceMock<MockPlatformProbeReport> report;
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("dummy"),mir::graphics::supported));
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("mesa"),mir::graphics::best))
        .Times(AtMost(1));
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("android"),mir::graphics::best))
        .Times(AtMost(1));

    auto module = mir::graphics::module_for_device(modules, report);
}

TEST_F(ServerPlatformProbe, LogsWithCorrectPriorityOnlyAndroidSupported)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_fails();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    NiceMock<MockPlatformProbeReport> report;
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("dummy"),mir::graphics::supported));
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("mesa"),mir::graphics::unsupported))
        .Times(AtMost(1));
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("android"),mir::graphics::best))
        .Times(AtMost(1));

    auto module = mir::graphics::module_for_device(modules, report);
}

TEST_F(ServerPlatformProbe, LogsWithCorrectPriorityOnlyMesaSupported)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_fails();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    NiceMock<MockPlatformProbeReport> report;
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("dummy"),mir::graphics::supported));
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("mesa"),mir::graphics::best))
        .Times(AtMost(1));
    EXPECT_CALL(report,
                module_probed(ModuleNameMatches("android"),mir::graphics::unsupported))
        .Times(AtMost(1));

    auto module = mir::graphics::module_for_device(modules, report);
}

TEST_F(ServerPlatformProbe, LogsSelectedPlugin)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    NiceMock<MockPlatformProbeReport> report;
    char const* selected_name;
    EXPECT_CALL(report, module_selected(_))
        .WillOnce(Invoke([&selected_name](mir::graphics::ModuleProperties const& descriptor)
                         {
                             selected_name = descriptor.name;
                         }));

    auto module = mir::graphics::module_for_device(modules, report);
    auto describe = module->load_function<mir::graphics::DescribeModule>("describe_module");
    EXPECT_STREQ(describe()->name, selected_name);
}

TEST_F(ServerPlatformProbe, IgnoresNonPlatformModules)
{
    using namespace testing;

    auto ensure_mesa = ensure_mesa_probing_succeeds();
    auto ensure_android = ensure_android_probing_succeeds();

    auto modules = available_platforms();
    add_dummy_platform(modules);

    modules.push_back(std::make_shared<mir::SharedLibrary>(mtf::library_path() +
                                                           "/libmirclient.so"));


    NiceMock<MockPlatformProbeReport> report;
    auto module = mir::graphics::module_for_device(modules, report);
    EXPECT_NE(nullptr, module);
}

TEST_F(ServerPlatformProbe, LogsModulesThatFailToLoad)
{
    using namespace testing;

    std::vector<std::shared_ptr<mir::SharedLibrary>> modules;
    modules.push_back(std::make_shared<mir::SharedLibrary>(mtf::library_path() +
                                                           "/libmirclient.so"));
    add_dummy_platform(modules);


    NiceMock<MockPlatformProbeReport> report;
    EXPECT_CALL(report, invalid_module_probed(_))
        .WillOnce(Invoke([](std::exception const& error)
    {
        EXPECT_THAT(error.what(), HasSubstr("libmirclient")) << "Name of the failing module";
        EXPECT_THAT(error.what(), HasSubstr("probe_platform")) << "Name of the expected function";
    }));
    auto module = mir::graphics::module_for_device(modules, report);
}
