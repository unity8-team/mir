/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "default_connection_configuration.h"

#include "display_configuration.h"
#include "rpc/make_rpc_channel.h"
#include "rpc/null_rpc_report.h"
#include "rpc/protobuf_protocol.h"
#include "mir/logging/dumb_console_logger.h"
#include "mir/input/input_platform.h"
#include "mir/input/null_input_receiver_report.h"
#include "logging/rpc_report.h"
#include "logging/input_receiver_report.h"
#include "mir/logging/shared_library_prober_report.h"
#include "mir/logging/null_shared_library_prober_report.h"
#include "lttng/rpc_report.h"
#include "lttng/input_receiver_report.h"
#include "lttng/shared_library_prober_report.h"
#include "connection_surface_map.h"
#include "lifecycle_control.h"
#include "mir/shared_library.h"
#include "mir/client_platform_factory.h"
#include "probing_client_platform_factory.h"
#include "mir_event_distributor.h"
#include "mir/shared_library_prober.h"
#include "rpc/stream_socket_transport.h"
#include "rpc/handshaking_connection_creator.h"
#include "rpc/rpc_channel_resolver.h"

#include <dlfcn.h>

namespace mcl = mir::client;


namespace
{
std::string const off_opt_val{"off"};
std::string const log_opt_val{"log"};
std::string const lttng_opt_val{"lttng"};

// Shove this here until we properly manage the lifetime of our
// loadable modules
std::shared_ptr<mcl::ProbingClientPlatformFactory> the_platform_prober;

// Hack around the way Qt loads mir:
// qtmir and therefore Mir are loaded via dlopen(..., RTLD_LOCAL).
// While this is sensible for a plugin it would mean that some symbols
// cannot be resolved by the Mir platform plugins. This hack makes the
// necessary symbols global.
void ensure_loaded_with_rtld_global()
{
    Dl_info info;

    // Cast dladdr itself to work around g++-4.8 warnings (LP: #1366134)
    typedef int (safe_dladdr_t)(void(*func)(), Dl_info *info);
    safe_dladdr_t *safe_dladdr = (safe_dladdr_t*)&dladdr;
    safe_dladdr(&ensure_loaded_with_rtld_global, &info);
    dlopen(info.dli_fname,  RTLD_NOW | RTLD_NOLOAD | RTLD_GLOBAL);
}
}

mcl::DefaultConnectionConfiguration::DefaultConnectionConfiguration(
    std::string const& socket_file)
    : socket_file{socket_file}
{
}

std::shared_ptr<mcl::ConnectionSurfaceMap>
mcl::DefaultConnectionConfiguration::the_surface_map()
{
    return surface_map([]
        {
            return std::make_shared<mcl::ConnectionSurfaceMap>();
        });
}

std::unique_ptr<mcl::rpc::RpcChannelResolver>
mcl::DefaultConnectionConfiguration::make_rpc_channel()
{
    mcl::rpc::HandshakingConnectionCreator handshake{make_supported_protocols()};
    return handshake.connect_to(mcl::rpc::transport_for(the_socket_file()));
}

std::vector<std::unique_ptr<mcl::rpc::ProtocolInterpreter>>
mcl::DefaultConnectionConfiguration::make_supported_protocols()
{
    std::vector<std::unique_ptr<mcl::rpc::ProtocolInterpreter>> protocols;
    protocols.push_back(std::make_unique<mcl::rpc::ProtobufProtocol>(the_surface_map(), the_display_configuration(), the_rpc_report(), the_lifecycle_control(), the_event_sink()));
    return protocols;
}

std::shared_ptr<mir::logging::Logger>
mcl::DefaultConnectionConfiguration::the_logger()
{
    return logger(
        []
        {
            return std::make_shared<mir::logging::DumbConsoleLogger>();
        });
}

std::shared_ptr<mcl::ClientPlatformFactory>
mcl::DefaultConnectionConfiguration::the_client_platform_factory()
{
    return client_platform_factory(
        [this]
        {
            ensure_loaded_with_rtld_global();
            auto const platform_override = getenv("MIR_CLIENT_PLATFORM_LIB");
            std::vector<std::shared_ptr<mir::SharedLibrary>> platform_plugins;
            if (platform_override)
            {
                platform_plugins.push_back(std::make_shared<mir::SharedLibrary>(platform_override));
            }
            else
            {
                auto const platform_path_override = getenv("MIR_CLIENT_PLATFORM_PATH");
                auto const platform_path = platform_path_override ? platform_path_override : MIR_CLIENT_PLATFORM_PATH;
                platform_plugins = mir::libraries_for_path(platform_path, *the_shared_library_prober_report());
            }

            the_platform_prober = std::make_shared<mcl::ProbingClientPlatformFactory>(platform_plugins);

            return the_platform_prober;
        });
}

std::shared_ptr<mir::input::receiver::InputPlatform>
mcl::DefaultConnectionConfiguration::the_input_platform()
{
    return input_platform(
        [this]
        {
            return mir::input::receiver::InputPlatform::create(the_input_receiver_report());
        });
}

std::string
mcl::DefaultConnectionConfiguration::the_socket_file()
{
    return socket_file;
}

std::shared_ptr<mcl::rpc::RpcReport>
mcl::DefaultConnectionConfiguration::the_rpc_report()
{
    return rpc_report(
        [this] () -> std::shared_ptr<mcl::rpc::RpcReport>
        {
            auto val_raw = getenv("MIR_CLIENT_RPC_REPORT");
            std::string const val{val_raw ? val_raw : off_opt_val};

            if (val == log_opt_val)
                return std::make_shared<mcl::logging::RpcReport>(the_logger());
            else if (val == lttng_opt_val)
                return std::make_shared<mcl::lttng::RpcReport>();
            else
                return std::make_shared<mcl::rpc::NullRpcReport>();
        });
}

std::shared_ptr<mir::input::receiver::InputReceiverReport>
mcl::DefaultConnectionConfiguration::the_input_receiver_report()
{
    return input_receiver_report(
        [this] () -> std::shared_ptr<mir::input::receiver::InputReceiverReport>
        {
            auto val_raw = getenv("MIR_CLIENT_INPUT_RECEIVER_REPORT");
            std::string const val{val_raw ? val_raw : off_opt_val};

            if (val == log_opt_val)
                return std::make_shared<mcl::logging::InputReceiverReport>(the_logger());
            else if (val == lttng_opt_val)
                return std::make_shared<mcl::lttng::InputReceiverReport>();
            else
                return std::make_shared<mir::input::receiver::NullInputReceiverReport>();
        });
}

std::shared_ptr<mcl::DisplayConfiguration> mcl::DefaultConnectionConfiguration::the_display_configuration()
{
    return display_configuration(
        []
        {
            return std::make_shared<mcl::DisplayConfiguration>();
        });
}

std::shared_ptr<mcl::LifecycleControl> mcl::DefaultConnectionConfiguration::the_lifecycle_control()
{
    return lifecycle_control(
        []
        {
            return std::make_shared<mcl::LifecycleControl>();
        });
}

std::shared_ptr<mcl::EventSink> mcl::DefaultConnectionConfiguration::the_event_sink()
{
    return event_distributor(
        []
        {
            return std::make_shared<MirEventDistributor>();
        });
}

std::shared_ptr<mcl::EventHandlerRegister> mcl::DefaultConnectionConfiguration::the_event_handler_register()
{
    return event_distributor(
        []
        {
            return std::make_shared<MirEventDistributor>();
        });
}

std::shared_ptr<mir::SharedLibraryProberReport> mir::client::DefaultConnectionConfiguration::the_shared_library_prober_report()
{
    return shared_library_prober_report(
        [this] () -> std::shared_ptr<mir::SharedLibraryProberReport>
        {
            auto val_raw = getenv("MIR_CLIENT_SHARED_LIBRARY_PROBER_REPORT");
            std::string const val{val_raw ? val_raw : off_opt_val};
            if (val == log_opt_val)
                return std::make_shared<mir::logging::SharedLibraryProberReport>(the_logger());
            else if (val == lttng_opt_val)
                return std::make_shared<mcl::lttng::SharedLibraryProberReport>();
            else
                return std::make_shared<mir::logging::NullSharedLibraryProberReport>();
        });
}
