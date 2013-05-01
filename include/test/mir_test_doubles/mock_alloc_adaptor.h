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

#ifndef MIR_TEST_DOUBLES_MOCK_ALLOC_ADAPTOR_H_
#define MIR_TEST_DOUBLES_MOCK_ALLOC_ADAPTOR_H_

#include "src/server/platform/graphics_buffer/android/graphic_alloc_adaptor.h"
#include "src/server/platform/graphics_buffer/android/android_buffer.h"

#include <system/window.h>
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockBufferHandle : public graphics::android::AndroidBufferHandle
{
public:
    MOCK_CONST_METHOD0(size,   geometry::Size());
    MOCK_CONST_METHOD0(stride, geometry::Stride());
    MOCK_CONST_METHOD0(format, geometry::PixelFormat());
    MOCK_CONST_METHOD0(usage,  graphics::android::BufferUsage());
    MOCK_CONST_METHOD0(get_ipc_package, std::shared_ptr<compositor::BufferIPCPackage>());
    MOCK_CONST_METHOD0(native_buffer_handle, std::shared_ptr<ANativeWindowBuffer>());
};

class MockAllocAdaptor : public graphics::android::GraphicAllocAdaptor
{
public:
    MOCK_METHOD3(alloc_buffer, std::shared_ptr<graphics::android::AndroidBufferHandle>(geometry::Size, geometry::PixelFormat, graphics::android::BufferUsage));
};

}
}
}

#endif /* MIR_TEST_DOUBLES_MOCK_ALLOC_ADAPTOR_H_ */
