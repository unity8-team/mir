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

#include "src/server/report/null_report_factory.h"
#include "mir_test_doubles/mock_display_report.h"
#include "src/platform/graphics/android/platform.h"
#include "mir_test_doubles/mock_buffer_ipc_message.h"
#include "mir_test_doubles/mock_android_hw.h"
#include "mir_test_doubles/fd_matcher.h"
#include "mir_test_doubles/stub_display_builder.h"
#include "mir_test_doubles/mock_android_native_buffer.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_android_alloc_device.h"
#include "mir_test/fake_shared.h"
#include <gtest/gtest.h>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mt=mir::test;
namespace mtd=mir::test::doubles;
namespace mr=mir::report;
namespace geom=mir::geometry;

TEST(PlatformBufferIPCPackaging, test_ipc_data_packed_correctly_for_nested)
{
    using namespace ::testing;
    unsigned int num_ints{43};
    unsigned int num_fds{55};
    int offset = 0;
    geom::Stride stride{300*4};
    geom::Size size{123, 456};
    mtd::MockBuffer mock_buffer;
    mtd::MockBufferIpcMessage mock_ipc_msg;

    auto native_buffer = std::make_shared<mtd::MockAndroidNativeBuffer>();
    auto native = mtd::generate_native_handle(num_ints, num_fds);
    ON_CALL(*native_buffer, handle())
        .WillByDefault(Return(native.get()));
    ON_CALL(mock_buffer, native_buffer_handle())
        .WillByDefault(Return(native_buffer));
    ON_CALL(mock_buffer, stride())
        .WillByDefault(Return(stride));
    ON_CALL(mock_buffer, size())
        .WillByDefault(Return(size));

    mga::Platform platform(std::make_shared<mtd::StubDisplayBuilder>(), mr::null_display_report());

    EXPECT_CALL(*native_buffer, copy_fence())
        .WillOnce(Return(-1));
    EXPECT_CALL(mock_ipc_msg, pack_data(static_cast<int>(mga::BufferFlag::unfenced)));
    for (auto i = 0u; i < num_fds; i++)
        EXPECT_CALL(mock_ipc_msg, pack_fd(mtd::RawFdMatcher(native->data[offset++])));
    for (auto i = 0u; i < num_ints; i++)
        EXPECT_CALL(mock_ipc_msg, pack_data(native->data[offset++]));
    EXPECT_CALL(mock_ipc_msg, pack_stride(stride));
    EXPECT_CALL(mock_ipc_msg, pack_size(size));

    platform.fill_buffer_package(&mock_ipc_msg, &mock_buffer, mg::BufferIpcMsgType::full_msg);
}

TEST(AndroidGraphicsPlatform, egl_native_display_is_egl_default_display)
{
    mga::Platform platform(
        std::make_shared<mtd::StubDisplayBuilder>(),
        mr::null_display_report());

    EXPECT_EQ(EGL_DEFAULT_DISPLAY, platform.egl_native_display());
}

TEST(NestedPlatformCreation, doesnt_access_display_hardware)
{
    using namespace testing;

    mtd::HardwareAccessMock hwaccess;
    mtd::MockDisplayReport stub_report;

    EXPECT_CALL(hwaccess, hw_get_module(StrEq(HWC_HARDWARE_MODULE_ID), _))
        .Times(0);
    EXPECT_CALL(hwaccess, hw_get_module(StrEq(GRALLOC_HARDWARE_MODULE_ID), _))
        .Times(AtMost(1));

    auto platform = mg::create_native_platform(mt::fake_shared(stub_report));
}
