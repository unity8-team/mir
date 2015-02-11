/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois<kevin.dubois@canonical.com>
 */

#include "mir_toolkit/mir_client_library.h"
#include "client_platform.h"
#include "client_buffer_factory.h"
#include "mesa_native_display_container.h"
#include "native_surface.h"
#include "mir/client_buffer_factory.h"
#include "mir/client_context.h"
#include "mir_toolkit/mesa/platform_operation.h"

#include <cstring>

namespace mcl=mir::client;
namespace mclm=mir::client::mesa;
namespace geom=mir::geometry;

namespace
{

struct NativeDisplayDeleter
{
    NativeDisplayDeleter(mcl::EGLNativeDisplayContainer& container)
    : container(container)
    {
    }
    void operator() (EGLNativeDisplayType* p)
    {
        auto display = *(reinterpret_cast<MirEGLNativeDisplayType*>(p));
        container.release(display);
        delete p;
    }

    mcl::EGLNativeDisplayContainer& container;
};

constexpr size_t division_ceiling(size_t a, size_t b)
{
    return ((a - 1) / b) + 1;
}

}

mclm::ClientPlatform::ClientPlatform(
        ClientContext* const context,
        std::shared_ptr<BufferFileOps> const& buffer_file_ops,
        mcl::EGLNativeDisplayContainer& display_container)
    : context{context},
      buffer_file_ops{buffer_file_ops},
      display_container(display_container),
      gbm_dev{nullptr}
{
}

std::shared_ptr<mcl::ClientBufferFactory> mclm::ClientPlatform::create_buffer_factory()
{
    return std::make_shared<mclm::ClientBufferFactory>(buffer_file_ops);
}

namespace
{
struct NativeWindowDeleter
{
    NativeWindowDeleter(mclm::NativeSurface* window)
     : window(window) {}

    void operator()(EGLNativeWindowType* type)
    {
        delete type;
        delete window;
    }

private:
    mclm::NativeSurface* window;
};
}

std::shared_ptr<EGLNativeWindowType> mclm::ClientPlatform::create_egl_native_window(EGLNativeSurface* client_surface)
{
    //TODO: this is awkward on both android and gbm...
    auto native_window = new NativeSurface(*client_surface);
    auto egl_native_window = new EGLNativeWindowType;
    *egl_native_window = reinterpret_cast<EGLNativeWindowType>(native_window);
    NativeWindowDeleter deleter(native_window);
    return std::shared_ptr<EGLNativeWindowType>(egl_native_window, deleter);
}

std::shared_ptr<EGLNativeDisplayType> mclm::ClientPlatform::create_egl_native_display()
{
    MirEGLNativeDisplayType *mir_native_display = new MirEGLNativeDisplayType;
    *mir_native_display = display_container.create(this);
    auto egl_native_display = reinterpret_cast<EGLNativeDisplayType*>(mir_native_display);

    return std::shared_ptr<EGLNativeDisplayType>(egl_native_display, NativeDisplayDeleter(display_container));
}

MirPlatformType mclm::ClientPlatform::platform_type() const
{
    return mir_platform_type_gbm;
}

void mclm::ClientPlatform::populate(MirPlatformPackage& package) const
{
    size_t constexpr pointer_size_in_ints = division_ceiling(sizeof(gbm_dev), sizeof(int));

    context->populate_server_package(package);

    auto const total_data_size = package.data_items + pointer_size_in_ints;
    if (gbm_dev && total_data_size <= mir_platform_package_max)
    {
        int gbm_ptr[pointer_size_in_ints]{};
        std::memcpy(&gbm_ptr, &gbm_dev, sizeof(gbm_dev));

        for (auto i : gbm_ptr)
            package.data[package.data_items++] = i;
    }
}

MirPlatformMessage* mclm::ClientPlatform::platform_operation(
    MirPlatformMessage const* msg)
{
    auto const op = mir_platform_message_get_opcode(msg);
    auto const msg_data = mir_platform_message_get_data(msg);

    if (op == MirMesaPlatformOperation::set_gbm_device &&
        msg_data.size == sizeof(MirMesaSetGBMDeviceRequest))
    {
        MirMesaSetGBMDeviceRequest set_gbm_device_request{nullptr};
        std::memcpy(&set_gbm_device_request, msg_data.data, msg_data.size);

        gbm_dev = set_gbm_device_request.device;

        static int const success{0};
        MirMesaSetGBMDeviceResponse const response{success};
        auto const response_msg = mir_platform_message_create(op);
        mir_platform_message_set_data(response_msg, &response, sizeof(response));

        return response_msg;
    }

    return nullptr;
}

MirNativeBuffer* mclm::ClientPlatform::convert_native_buffer(graphics::NativeBuffer* buf) const
{
    //MirNativeBuffer is type-compatible with the MirNativeBuffer
    return buf;
}
