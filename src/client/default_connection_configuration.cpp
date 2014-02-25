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

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include "default_connection_configuration.h"

#include "display_configuration.h"
#include "rpc/mir_socket_rpc_channel.h"
#include "rpc/null_rpc_report.h"
#include "mir/logging/dumb_console_logger.h"
#include "mir/input/input_platform.h"
#include "mir/input/null_input_receiver_report.h"
#include "logging/rpc_report.h"
#include "logging/input_receiver_report.h"
#include "lttng/rpc_report.h"
#include "lttng/input_receiver_report.h"
#include "connection_surface_map.h"
#include "lifecycle_control.h"
#include "mir/shared_library.h"
#include "client_platform_factory.h"

namespace mcl = mir::client;

namespace
{
std::string const off_opt_val{"off"};
std::string const log_opt_val{"log"};
std::string const lttng_opt_val{"lttng"};
std::string const default_platform_lib{"libmirclientplatform.so"};

mir::SharedLibrary const* load_library(std::string const& libname)
{
    // There's no point in loading twice, and it isn't safe to unload...
    static std::map<std::string, std::shared_ptr<mir::SharedLibrary>> libraries_cache;

    if (auto& ptr = libraries_cache[libname])
    {
        return ptr.get();
    }
    else
    {
        ptr = std::make_shared<mir::SharedLibrary>(libname);
        return ptr.get();
    }
}
}

namespace
{
struct Prefix
{
    template<int Size>
    Prefix(char const (&prefix)[Size]) : size(Size-1), prefix(prefix) {}

    bool is_start_of(std::string const& name) const
    { return !strncmp(name.c_str(), prefix, size); }

    int const size;
    char const* const prefix;
} const fd_prefix("fd://");
}


mcl::DefaultConnectionConfiguration::DefaultConnectionConfiguration(
    std::string const& socket_file)
    : owns_socket{false}
{
    if (fd_prefix.is_start_of(socket_file))
    {
        socket_fd = atoi(socket_file.c_str()+fd_prefix.size);
    }
    else
    {
        struct sockaddr_un addr;
        addr.sun_family = AF_UNIX;
        strcpy(addr.sun_path, socket_file.c_str());

        socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (socket_fd < 0)
        {
            BOOST_THROW_EXCEPTION(
                        boost::enable_error_info(
                            std::runtime_error(std::string("Failed to create socket: ") +
                                               strerror(errno)))<<boost::errinfo_errno(errno));
        }
        owns_socket = true;
        if (connect(socket_fd, (struct sockaddr* const)&addr, sizeof addr) < 0)
        {
            BOOST_THROW_EXCEPTION(
                        boost::enable_error_info(
                            std::runtime_error(std::string("Failed to connect to server socket: ") +
                                               strerror(errno)))<<boost::errinfo_errno(errno));
        }
    }
}

mir::client::DefaultConnectionConfiguration::~DefaultConnectionConfiguration()
{
    if (owns_socket)
    {
        close(socket_fd);
    }
}

std::shared_ptr<mcl::ConnectionSurfaceMap>
mcl::DefaultConnectionConfiguration::the_surface_map()
{
    return surface_map([]
        {
            return std::make_shared<mcl::ConnectionSurfaceMap>();
        });
}

std::shared_ptr<google::protobuf::RpcChannel>
mcl::DefaultConnectionConfiguration::the_rpc_channel()
{
    return rpc_channel(
        [this]
        {
            // MirSocketRpcChannel takes lifetime responsibility for the socket fd
            owns_socket = false;
            return std::make_shared<mcl::rpc::MirSocketRpcChannel>(
                the_socket_fd(), the_surface_map(), the_display_configuration(), the_rpc_report(), the_lifecycle_control());
        });
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
        []
        {
            auto const val_raw = getenv("MIR_CLIENT_PLATFORM_LIB");
            std::string const val{val_raw ? val_raw : default_platform_lib};
            auto const platform_lib = load_library(val);

            auto const create_client_platform_factory =
                platform_lib->load_function<mcl::CreateClientPlatformFactory>(
                    "create_client_platform_factory");

            return create_client_platform_factory();
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

int
mcl::DefaultConnectionConfiguration::the_socket_fd()
{
    return socket_fd;
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
