/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/graphics/buffer.h"
#include "mir/graphics/buffer_ipc_message.h"
#include "mir/graphics/platform_ipc_package.h"
#include "mir/graphics/nested_context.h"
#include "mir/graphics/platform_operation_message.h"
#include "display_helpers.h"
#include "drm_authentication.h"
#include "drm_close_threadsafe.h"
#include "ipc_operations.h"
#include "mir_toolkit/mesa/platform_operation.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/exception/errinfo_errno.hpp>

namespace mg = mir::graphics;
namespace mgm = mir::graphics::mesa;

namespace
{
struct MesaPlatformIPCPackage : public mg::PlatformIPCPackage
{
    MesaPlatformIPCPackage(int drm_auth_fd)
    {
        ipc_fds.push_back(drm_auth_fd);
    }

    ~MesaPlatformIPCPackage()
    {
        if (ipc_fds.size() > 0 && ipc_fds[0] >= 0)
            mgm::drm_close_threadsafe(ipc_fds[0]);
    }
};
}

mgm::IpcOperations::IpcOperations(std::shared_ptr<DRMAuthentication> const& drm_auth) :
    drm_auth{drm_auth}
{
}

void mgm::IpcOperations::pack_buffer(
    mg::BufferIpcMessage& packer, Buffer const& buffer, BufferIpcMsgType msg_type) const
{
    if (msg_type == mg::BufferIpcMsgType::full_msg)
    {
        auto native_handle = buffer.native_buffer_handle();
        for(auto i=0; i<native_handle->data_items; i++)
        {
            packer.pack_data(native_handle->data[i]);
        }
        for(auto i=0; i<native_handle->fd_items; i++)
        {
            packer.pack_fd(mir::Fd(IntOwnedFd{native_handle->fd[i]}));
        }

        packer.pack_stride(buffer.stride());
        packer.pack_flags(native_handle->flags);
        packer.pack_size(buffer.size());
    }
}

void mgm::IpcOperations::unpack_buffer(BufferIpcMessage&, Buffer const&) const
{
}

mg::PlatformOperationMessage mgm::IpcOperations::platform_operation(
    unsigned int const op, mg::PlatformOperationMessage const& request)
{
    if (op == MirMesaPlatformOperation::auth_magic)
    {
        MirMesaAuthMagicRequest auth_magic_request;
        if (request.data.size() == sizeof(auth_magic_request))
        {
            auth_magic_request =
                *reinterpret_cast<decltype(auth_magic_request) const*>(request.data.data());
        }
        else
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error("Invalid request message for auth_magic platform operation"));
        }

        mg::PlatformOperationMessage response;
        response.data.resize(sizeof(MirMesaAuthMagicResponse));
        auto const auth_magic_response_ptr =
            reinterpret_cast<MirMesaAuthMagicResponse*>(response.data.data());

        try
        {
            drm_auth->auth_magic(auth_magic_request.magic);
            auth_magic_response_ptr->status = 0;
        }
        catch (std::exception const& e)
        {
            auto errno_ptr = boost::get_error_info<boost::errinfo_errno>(e);

            if (errno_ptr != nullptr)
                auth_magic_response_ptr->status = *errno_ptr;
            else
                throw;
        }

        return response;
    }
    else if (op == MirMesaPlatformOperation::auth_fd)
    {
        if (request.data.size() != 0 || request.fds.size() != 0)
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error("Invalid request message for auth_fd platform operation"));
        }

        return mg::PlatformOperationMessage{{},{drm_auth->authenticated_fd()}};
    }
    else
    {
        BOOST_THROW_EXCEPTION(
            std::runtime_error("Invalid platform operation"));
    }
}

std::shared_ptr<mg::PlatformIPCPackage> mgm::IpcOperations::connection_ipc_package()
{
    return std::make_shared<MesaPlatformIPCPackage>(drm_auth->authenticated_fd());
}
