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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_CLIENT_PENDING_CALL_CACHE_H_
#define MIR_CLIENT_PENDING_CALL_CACHE_H_

#include "mir_logger.h"
#include "mir/unique_id_generator.h"

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include <memory>
#include <map>
#include <iosfwd>
#include <mutex>
#include <atomic>

namespace mir
{
namespace protobuf
{
namespace wire
{
class Invocation;
class Result;
}
}

namespace client
{
namespace detail
{
typedef std::vector<char> SendBuffer;

class PendingCallCache : public UniqueIdGenerator
{
public:
    PendingCallCache(std::shared_ptr<Logger> const& log);

    SendBuffer& save_completion_details(
        mir::protobuf::wire::Invocation& invoke,
        google::protobuf::Message* response,
        std::shared_ptr<google::protobuf::Closure> const& complete);


    void complete_response(mir::protobuf::wire::Result& result);

    bool empty() const;

    bool id_in_use(UniqueIdGenerator::Id x) const;

private:

    struct PendingCall
    {
        PendingCall(
            google::protobuf::Message* response,
            std::shared_ptr<google::protobuf::Closure> const& target)
        : response(response), complete(target) {}

        PendingCall()
        : response(0), complete() {}

        SendBuffer send_buffer;
        google::protobuf::Message* response;
        std::shared_ptr<google::protobuf::Closure> complete;
    };

    std::mutex mutable mutex;
    std::map<int, PendingCall> pending_calls;
    std::shared_ptr<Logger> const log;
};
}
}
}

#endif /* MIR_CLIENT_PENDING_CALL_CACHE_H_ */
