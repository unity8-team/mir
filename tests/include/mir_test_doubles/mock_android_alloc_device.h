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
#ifndef MIR_TEST_DOUBLES_MOCK_ANDROID_ALLOC_DEVICE_H_
#define MIR_TEST_DOUBLES_MOCK_ANDROID_ALLOC_DEVICE_H_

#include <memory>
#include <gmock/gmock.h>
#include <hardware/gralloc.h>

namespace mir
{
namespace test
{
namespace doubles
{
std::shared_ptr<native_handle_t> generate_native_handle(size_t num_ints, size_t num_fds);
struct MockAllocDevice : alloc_device_t
{
    MockAllocDevice() :
        buffer_handle{generate_native_handle(43, 22)},
        fake_stride{300}
    {
        using namespace testing;
        alloc = hook_alloc;
        free = hook_free;
        dump = hook_dump;
        ON_CALL(*this, alloc_interface(_,_,_,_,_,_,_))
            .WillByDefault(DoAll(
                           SetArgPointee<5>(buffer_handle.get()),
                           SetArgPointee<6>(fake_stride),
                           Return(0)));
        ON_CALL(*this, free_interface(_,_))
            .WillByDefault(Return(0));

    }

    static int hook_alloc(alloc_device_t* mock_alloc,
                          int w, int h, int format, int usage,
                          buffer_handle_t* handle, int* stride)
    {
        MockAllocDevice* mocker = static_cast<MockAllocDevice*>(mock_alloc);
        return mocker->alloc_interface(mock_alloc, w, h, format, usage, handle, stride);
    }

    static int hook_free(alloc_device_t* mock_alloc, buffer_handle_t handle)
    {
        MockAllocDevice* mocker = static_cast<MockAllocDevice*>(mock_alloc);
        return mocker->free_interface(mock_alloc, handle);
    }

    static void hook_dump(alloc_device_t* mock_alloc, char* buf, int buf_len)
    {
        MockAllocDevice* mocker = static_cast<MockAllocDevice*>(mock_alloc);
        mocker->dump_interface(mock_alloc, buf, buf_len);
    }

    MOCK_METHOD7(alloc_interface,  int(alloc_device_t*, int, int, int, int, buffer_handle_t*, int*));
    MOCK_METHOD2(free_interface, int(alloc_device_t*, buffer_handle_t));
    MOCK_METHOD3(dump_interface, int(alloc_device_t*, char*, int));

    std::shared_ptr<native_handle_t> const buffer_handle;
    unsigned int fake_stride;
};
}
}
} // namespace mir

#endif /* MIR_TEST_DOUBLES_MOCK_ANDROID_ALLOC_DEVICE_H_ */
