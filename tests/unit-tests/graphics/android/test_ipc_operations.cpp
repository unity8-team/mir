/*

 * Copyright © 2012-2014 Canonical Ltd.
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
#include "mir/graphics/native_platform.h"
#include "mir/graphics/platform_ipc_operations.h"
#include "mir/options/program_option.h"
#include "src/platform/graphics/android/platform.h"
#include "src/platform/graphics/android/ipc_operations.h"
#include "mir_test_doubles/mock_buffer.h"
#include "mir_test_doubles/mock_android_hw.h"
#include "mir_test_doubles/mock_buffer_ipc_message.h"
#include "mir_test_doubles/mock_display_report.h"
#include "mir_test_doubles/stub_display_builder.h"
#include "mir_test_doubles/mock_android_native_buffer.h"
#include "mir_test_doubles/stub_android_native_buffer.h"
#include "mir_test_doubles/fd_matcher.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_android_native_buffer.h"
#include "mir_test_doubles/mock_android_alloc_device.h"
#include <gtest/gtest.h>

namespace mg=mir::graphics;
namespace mga=mir::graphics::android;
namespace mt=mir::test;
namespace mtd=mir::test::doubles;
namespace mr=mir::report;
namespace geom=mir::geometry;
namespace mo=mir::options;

struct IpcOperations : testing::Test
{
    IpcOperations() :
        stride{300*4},
        num_ints{30},
        num_fds{12},
        native_buffer{std::make_shared<mtd::MockAndroidNativeBuffer>()},
        native_buffer_handle{mtd::generate_native_handle(num_ints, num_fds)},
        mock_buffer{std::make_shared<testing::NiceMock<mtd::MockBuffer>>()}
    {
        using namespace testing;
        ON_CALL(*native_buffer, handle())
            .WillByDefault(Return(native_buffer_handle.get()));
        ON_CALL(*mock_buffer, native_buffer_handle())
            .WillByDefault(Return(native_buffer));
        ON_CALL(*mock_buffer, stride())
            .WillByDefault(Return(stride));
    }

    geom::Stride stride;
    size_t num_ints;
    size_t num_fds;
    std::shared_ptr<mtd::MockAndroidNativeBuffer> native_buffer;
    std::shared_ptr<native_handle_t> native_buffer_handle;
    std::shared_ptr<mtd::MockBuffer> mock_buffer;
    std::shared_ptr<mg::DisplayReport> stub_display_report;
    mga::IpcOperations ipc_operations;
};

/* ipc packaging tests */
TEST_F(IpcOperations, test_ipc_data_packed_correctly_for_full_ipc_with_fence)
{
    using namespace ::testing;
    int fake_fence{333};
    EXPECT_CALL(*native_buffer, copy_fence())
        .WillOnce(Return(fake_fence));

    mtd::MockBufferIpcMessage mock_ipc_msg;
    int offset = 0;
    EXPECT_CALL(mock_ipc_msg, pack_data(static_cast<int>(mga::BufferFlag::fenced)));
    EXPECT_CALL(mock_ipc_msg, pack_fd(mtd::RawFdMatcher(fake_fence)));
    for (auto i = 0u; i < num_fds; i++)
        EXPECT_CALL(mock_ipc_msg, pack_fd(mtd::RawFdMatcher(native_buffer_handle->data[offset++])));
    for (auto i = 0u; i < num_ints; i++)
        EXPECT_CALL(mock_ipc_msg, pack_data(native_buffer_handle->data[offset++]));

    EXPECT_CALL(*mock_buffer, stride())
        .WillOnce(Return(stride));
    EXPECT_CALL(mock_ipc_msg, pack_stride(stride))
        .Times(1);

    EXPECT_CALL(*mock_buffer, size())
        .WillOnce(Return(mir::geometry::Size{123, 456}));
    EXPECT_CALL(mock_ipc_msg, pack_size(_))
        .Times(1);

    ipc_operations.pack_buffer(mock_ipc_msg, *mock_buffer, mg::BufferIpcMsgType::full_msg);
}

TEST_F(IpcOperations, test_ipc_data_packed_correctly_for_full_ipc_without_fence)
{
    using namespace ::testing;
    EXPECT_CALL(*native_buffer, copy_fence())
        .WillOnce(Return(-1));

    mtd::MockBufferIpcMessage mock_ipc_msg;
    int offset = 0;
    EXPECT_CALL(mock_ipc_msg, pack_data(static_cast<int>(mga::BufferFlag::unfenced)));
    EXPECT_CALL(mock_ipc_msg, pack_fd(mtd::RawFdMatcher(-1)))
        .Times(0);

    for (auto i = 0u; i < num_fds; i++)
    {
        EXPECT_CALL(mock_ipc_msg, pack_fd(mtd::RawFdMatcher(native_buffer_handle->data[offset++])))
            .Times(1);
    }
    for (auto i = 0u; i < num_ints; i++)
    {
        EXPECT_CALL(mock_ipc_msg, pack_data(native_buffer_handle->data[offset++]))
            .Times(1);
    }

    EXPECT_CALL(*mock_buffer, stride())
        .WillOnce(Return(stride));
    EXPECT_CALL(mock_ipc_msg, pack_stride(stride))
        .Times(1);

    EXPECT_CALL(*mock_buffer, size())
        .WillOnce(Return(mir::geometry::Size{123, 456}));
    EXPECT_CALL(mock_ipc_msg, pack_size(_))
        .Times(1);

    ipc_operations.pack_buffer(mock_ipc_msg, *mock_buffer, mg::BufferIpcMsgType::full_msg);
}

TEST_F(IpcOperations, test_ipc_data_packed_correctly_for_partial_ipc)
{
    using namespace ::testing;

    int fake_fence{33};
    mtd::MockBufferIpcMessage mock_ipc_msg;

    Sequence seq;
    EXPECT_CALL(mock_ipc_msg, pack_data(static_cast<int>(mga::BufferFlag::fenced)))
        .InSequence(seq);
    EXPECT_CALL(mock_ipc_msg, pack_fd(mtd::RawFdMatcher(fake_fence)))
        .InSequence(seq);
    EXPECT_CALL(mock_ipc_msg, pack_data(static_cast<int>(mga::BufferFlag::unfenced)))
        .InSequence(seq);

    EXPECT_CALL(*native_buffer, copy_fence())
        .Times(2)
        .WillOnce(Return(fake_fence))
        .WillOnce(Return(-1));

    ipc_operations.pack_buffer(mock_ipc_msg, *mock_buffer, mg::BufferIpcMsgType::update_msg);
    ipc_operations.pack_buffer(mock_ipc_msg, *mock_buffer, mg::BufferIpcMsgType::update_msg);
}

TEST_F(IpcOperations, unpacks_fenced_buffer_update)
{
    using namespace ::testing;
    mtd::MockBufferIpcMessage mock_ipc_msg;
    mir::Fd stub_fd{fileno(tmpfile())};

    EXPECT_CALL(mock_ipc_msg, data())
        .WillOnce(Return(std::vector<int>{static_cast<int>(mga::BufferFlag::fenced)}));
    EXPECT_CALL(mock_ipc_msg, fds())
        .WillOnce(Return(std::vector<mir::Fd>{stub_fd}));
    EXPECT_CALL(*native_buffer, update_usage(_, mga::BufferAccess::write))
        .Times(1);
    ipc_operations.unpack_buffer(mock_ipc_msg, *mock_buffer);
}

TEST_F(IpcOperations, unpacks_unfenced_buffer)
{
    using namespace ::testing;
    mtd::MockBufferIpcMessage mock_ipc_msg;
    EXPECT_CALL(mock_ipc_msg, data())
        .WillOnce(Return(std::vector<int>{static_cast<int>(mga::BufferFlag::unfenced)}));
    EXPECT_CALL(mock_ipc_msg, fds())
        .WillOnce(Return(std::vector<mir::Fd>{}));
    EXPECT_CALL(*native_buffer, update_usage(_, _))
        .Times(0);
    ipc_operations.unpack_buffer(mock_ipc_msg, *mock_buffer);
}
