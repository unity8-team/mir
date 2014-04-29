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

#include "src/server/graphics/nested/nested_platform.h"

#include "mir/graphics/native_platform.h"
#include "src/server/report/null/display_report.h"

#include "mir_test_doubles/stub_host_connection.h"
#include "mir_test_doubles/null_event_filter.h"
#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace mgn = mir::graphics::nested;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

class StubHostConnection : public mtd::StubHostConnection
{
public:
    EGLNativeDisplayType egl_native_display() override
    {
        return reinterpret_cast<EGLNativeDisplayType>(&native_display);
    }

    int native_display = 0;
};

class StubNativePlatform : public mg::NativePlatform
{
public:
    void initialize(std::shared_ptr<mg::NestedContext> const&) override {}

    std::shared_ptr<mg::GraphicBufferAllocator> create_buffer_allocator(
        std::shared_ptr<mg::BufferInitializer> const&) override
    {
        return {};
    }

    std::shared_ptr<mg::PlatformIPCPackage> get_ipc_package() override
    {
        return {};
    }

    std::shared_ptr<mg::InternalClient> create_internal_client() override
    {
        return {};
    }

    void fill_ipc_package(mg::BufferIPCPacker*, mg::Buffer const*) const override {}
};

TEST(NestedGraphicsPlatform, egl_native_display_is_host_connection_native_display)
{
    using namespace testing;

    StubHostConnection host_connection;

    mgn::NestedPlatform nested_platform{
        mt::fake_shared(host_connection),
        std::make_shared<mtd::NullEventFilter>(),
        std::make_shared<mir::report::null::DisplayReport>(),
        std::make_shared<StubNativePlatform>()};

    EXPECT_THAT(nested_platform.egl_native_display(),
                Eq(host_connection.egl_native_display()));
}
