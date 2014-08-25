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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_FRONTEND_PROTOBUF_BUFFER_PACKER_H_
#define MIR_FRONTEND_PROTOBUF_BUFFER_PACKER_H_

#include "mir/graphics/buffer_ipc_packer.h"
#include "mir_protobuf.pb.h"
#include <memory>

namespace mir
{
namespace graphics
{
class DisplayConfiguration;
}
namespace frontend
{
class MessageResourceCache;
namespace detail
{

void pack_protobuf_display_configuration(protobuf::DisplayConfiguration& protobuf_config,
                                         graphics::DisplayConfiguration const& display_config);

class ProtobufBufferPacker : public graphics::BufferIPCPacker
{
public:
    ProtobufBufferPacker(protobuf::Buffer*, std::shared_ptr<MessageResourceCache> const&);
    void pack_fd(Fd const&);
    void pack_data(int);
    void pack_stride(geometry::Stride);
    void pack_flags(unsigned int);
    void pack_size(geometry::Size const& size);
private:
    protobuf::Buffer* buffer_response;
    std::shared_ptr<MessageResourceCache> const resource_cache;    
};

}
}
}

#endif /* MIR_FRONTEND_PROTOBUF_BUFFER_PACKER_H_ */