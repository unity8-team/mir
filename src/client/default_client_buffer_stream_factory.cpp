/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */
#include "default_client_buffer_stream_factory.h"
#include "buffer_stream.h"
#include "perf_report.h"
#include "logging/perf_report.h"

namespace mcl = mir::client;
namespace ml = mir::logging;
namespace mp = mir::protobuf;

namespace
{

std::shared_ptr<mcl::PerfReport>
make_perf_report(std::shared_ptr<ml::Logger> const& logger)
{
    // TODO: It seems strange that this directly uses getenv
    const char* report_target = getenv("MIR_CLIENT_PERF_REPORT");
    if (report_target && !strncmp(report_target, "log", strlen(report_target)))
    {
        return std::make_shared<mcl::logging::PerfReport>(logger);
    }
    else
    {
        return std::make_shared<mcl::NullPerfReport>();
    }
}

}

mcl::DefaultClientBufferStreamFactory::DefaultClientBufferStreamFactory(
    std::shared_ptr<mcl::ClientPlatform> const& client_platform,
    std::shared_ptr<ml::Logger> const& logger)
    : client_platform(client_platform),
      logger(logger)
{
}

std::shared_ptr<mcl::ClientBufferStream> mcl::DefaultClientBufferStreamFactory::make_consumer_stream(mp::DisplayServer& server,
    mp::BufferStream const& protobuf_bs, std::string const& surface_name)
{
    return std::make_shared<mcl::BufferStream>(server, mcl::BufferStreamMode::Consumer, client_platform, protobuf_bs, make_perf_report(logger), surface_name);
}

std::shared_ptr<mcl::ClientBufferStream> mcl::DefaultClientBufferStreamFactory::make_producer_stream(mp::DisplayServer& server,
    mp::BufferStream const& protobuf_bs, std::string const& surface_name)
{
    return std::make_shared<mcl::BufferStream>(server, mcl::BufferStreamMode::Producer, client_platform, protobuf_bs, make_perf_report(logger), surface_name);
}
