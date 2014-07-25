/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
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

#ifndef MIR_TEST_DOUBLES_MOCK_ANDROID_NATIVE_BUFFER_H_
#define MIR_TEST_DOUBLES_MOCK_ANDROID_NATIVE_BUFFER_H_

#include "mir/graphics/android/native_buffer.h"
#include "mir/geometry/size.h"
#include "mir_test/gmock_fd_fix.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockAndroidNativeBuffer : public graphics::NativeBuffer
{
    MockAndroidNativeBuffer()
    {
        using namespace testing;
        ON_CALL(*this, anwb())
            .WillByDefault(Return(&stub_anwb));
        ON_CALL(*this, handle())
            .WillByDefault(Return(&native_handle));
    }

    MockAndroidNativeBuffer(geometry::Size sz)
        : MockAndroidNativeBuffer()
    {
        stub_anwb.width = sz.width.as_int();
        stub_anwb.height = sz.height.as_int();
    }

    void update_usage(Fd fd, graphics::android::BufferAccess access)
    {
        update_usage_(fd, access);
    }

    MOCK_CONST_METHOD0(anwb, ANativeWindowBuffer*());
    MOCK_CONST_METHOD0(handle, buffer_handle_t());
    MOCK_CONST_METHOD0(copy_fence, Fd());

    MOCK_METHOD1(ensure_available_for, void(graphics::android::BufferAccess));
    MOCK_METHOD2(update_usage_, void(Fd&, graphics::android::BufferAccess));

    ANativeWindowBuffer stub_anwb;
    native_handle_t native_handle;

};

typedef testing::NiceMock<MockAndroidNativeBuffer> StubAndroidNativeBuffer;

}
}
}

#endif /* MIR_TEST_DOUBLES_MOCK_ANDROID_NATIVE_BUFFER_H_ */
