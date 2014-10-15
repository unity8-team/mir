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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "vsync_simulating_graphics_platform.h"

#include "mir/graphics/buffer_writer.h"
#include "mir/graphics/platform_ipc_operations.h"
#include "mir/graphics/platform_ipc_package.h"

#include "mir_test_doubles/stub_buffer_allocator.h"

namespace mg = mir::graphics;
namespace mtd = mir::test::doubles;

namespace
{

struct StubBufferWriter : public mg::BufferWriter
{
    void write(mg::Buffer &, unsigned char const*, size_t) override
    {
    }
};

class StubIpcOps : public mg::PlatformIpcOperations
{
    void pack_buffer(
        mg::BufferIpcMessage&,
        mg::Buffer const&,
        mg::BufferIpcMsgType) const override
    {
    }

    void unpack_buffer(
        mg::BufferIpcMessage&, mg::Buffer const&) const override
    {
    }

    std::shared_ptr<mg::PlatformIPCPackage> connection_ipc_package() override
    {
        return std::make_shared<mg::PlatformIPCPackage>();
    }
};

}

std::shared_ptr<mg::GraphicBufferAllocator> VsyncSimulatingPlatform::create_buffer_allocator(
    std::shared_ptr<mg::BufferInitializer> const&)
{
    return std::make_shared<mtd::StubBufferAllocator>();
}

std::shared_ptr<mg::BufferWriter> VsyncSimulatingPlatform::make_buffer_writer()
{
    return std::make_shared<StubBufferWriter>();
}
    
// TODO: Pass size and rate...
std::shared_ptr<mg::Display> VsyncSimulatingPlatform::create_display(
    std::shared_ptr<mg::DisplayConfigurationPolicy> const& initial_conf_policy,
    std::shared_ptr<mg::GLProgramFactory> const& gl_program_factory,
     std::shared_ptr<mg::GLConfig> const& gl_config)
{
    (void) initial_conf_policy;
    (void) gl_program_factory;
    (void) gl_config;
    return nullptr;
}
    
std::shared_ptr<mg::PlatformIpcOperations> VsyncSimulatingPlatform::make_ipc_operations() const
{
    return std::make_shared<StubIpcOps>();
}

std::shared_ptr<mg::InternalClient> VsyncSimulatingPlatform::create_internal_client()
{
    return nullptr;
}
