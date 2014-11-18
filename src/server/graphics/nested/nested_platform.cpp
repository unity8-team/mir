/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Eleni Maria Stea <elenimaria.stea@canonical.com>
 */

#include "nested_platform.h"
#include "nested_vsync_provider.h"
#include "host_connection.h"
#include "mir/graphics/nested_context.h"

#include "nested_display.h"

namespace mg = mir::graphics;
namespace mgn = mir::graphics::nested;
namespace mf = mir::frontend;

mgn::NestedPlatform::NestedPlatform(
    std::shared_ptr<HostConnection> const& connection,
    std::shared_ptr<input::InputDispatcher> const& dispatcher,
    std::shared_ptr<mg::DisplayReport> const& display_report,
    std::shared_ptr<mg::NativePlatform> const& native_platform) :
    native_platform{native_platform},
    dispatcher{dispatcher},
    display_report{display_report},
    connection{connection}
{
}

std::shared_ptr<mg::GraphicBufferAllocator> mgn::NestedPlatform::create_buffer_allocator()
{
    return native_platform->create_buffer_allocator();
}

std::shared_ptr<mg::BufferWriter> mgn::NestedPlatform::make_buffer_writer()
{
    return native_platform->make_buffer_writer();
}

std::shared_ptr<mg::Display> mgn::NestedPlatform::create_display(
    std::shared_ptr<mg::DisplayConfigurationPolicy> const& conf_policy,
    std::shared_ptr<mg::GLProgramFactory> const&,
    std::shared_ptr<mg::GLConfig> const& gl_config)
{
    return std::make_shared<mgn::NestedDisplay>(
        shared_from_this(), connection, dispatcher, display_report, conf_policy, gl_config);
}

std::shared_ptr<mg::InternalClient> mgn::NestedPlatform::create_internal_client()
{
    return native_platform->create_internal_client();
}

namespace
{
class BufferPacker : public mg::PlatformIpcOperations
{
public:
    BufferPacker(std::shared_ptr<mg::NativePlatform> const& native_platform) :
        native_platform{native_platform}
    {}
    void pack_buffer(
        mg::BufferIpcMessage& message, mg::Buffer const& buffer, mg::BufferIpcMsgType msg_type) const override
    {
        native_platform->fill_buffer_package(&message, &buffer, msg_type);
    }

    void unpack_buffer(mg::BufferIpcMessage&, mg::Buffer const&) const override {}

    std::shared_ptr<mg::PlatformIPCPackage> connection_ipc_package() override
    {
        return native_platform->connection_ipc_package();
    }

    mg::PlatformIPCPackage platform_operation(unsigned int const, mg::PlatformIPCPackage const&) override
    {
        return mg::PlatformIPCPackage();
    }

private:
    std::shared_ptr<mg::NativePlatform> const native_platform;
};
}

std::shared_ptr<mg::PlatformIpcOperations> mgn::NestedPlatform::make_ipc_operations() const
{
    return std::make_shared<BufferPacker>(native_platform);
}

EGLNativeDisplayType mgn::NestedPlatform::egl_native_display() const
{
    return connection->egl_native_display();
}
