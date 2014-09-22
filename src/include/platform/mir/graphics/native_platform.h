/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Eleni Maria Stea <elenimaria.stea@canonical.com>
 */
#ifndef MIR_GRAPHICS_NATIVE_PLATFORM_H_
#define MIR_GRAPHICS_NATIVE_PLATFORM_H_

#include "mir/graphics/platform.h"
#include <memory>
#include <functional>

namespace mir
{
namespace options
{
class Option;
}
namespace graphics
{
class GraphicBufferAllocator;
class BufferInitializer;
class PlatformIPCPackage;
class InternalClient;
class BufferIPCPacker;
class Buffer;
class BufferWriter;
class DisplayReport;
class NestedContext;

class NativePlatform
{
public:
    NativePlatform() {}

    virtual void initialize(std::shared_ptr<NestedContext> const& nested_context) = 0;

    virtual std::shared_ptr<GraphicBufferAllocator> create_buffer_allocator(
        std::shared_ptr<BufferInitializer> const& buffer_initializer) = 0;

    virtual std::shared_ptr<PlatformIPCPackage> get_ipc_package() = 0;

    virtual std::shared_ptr<InternalClient> create_internal_client() = 0;

    virtual void fill_buffer_package(
        BufferIPCPacker* packer,
        Buffer const* buffer,
        BufferIpcMsgType msg_type) const = 0;

    virtual std::shared_ptr<BufferWriter> make_buffer_writer() = 0;

    virtual ~NativePlatform() = default;
    NativePlatform(NativePlatform const&) = delete;
    NativePlatform& operator=(NativePlatform const&) = delete;
};

extern "C" typedef std::shared_ptr<NativePlatform>(*CreateNativePlatform)(std::shared_ptr<DisplayReport> const& report);
extern "C" std::shared_ptr<NativePlatform> create_native_platform(std::shared_ptr<DisplayReport> const& report);
}
}

#endif // MIR_GRAPHICS_NATIVE_PLATFORM_H_
