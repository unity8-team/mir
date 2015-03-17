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
 * Authored by: Thomas Guest <thomas.guest@canonical.com>
 */

#define MIR_INCLUDE_DEPRECATED_EVENT_HEADER

#include "mir_connection.h"
#include "mir_surface.h"
#include "mir_prompt_session.h"
#include "default_client_buffer_stream_factory.h"
#include "mir_toolkit/mir_platform_message.h"
#include "mir/client_platform.h"
#include "mir/client_platform_factory.h"
#include "rpc/mir_basic_rpc_channel.h"
#include "mir/dispatch/dispatchable.h"
#include "mir/dispatch/simple_dispatch_thread.h"
#include "connection_configuration.h"
#include "display_configuration.h"
#include "connection_surface_map.h"
#include "lifecycle_control.h"

#include "mir/events/event_builders.h"
#include "mir/logging/logger.h"

#include <algorithm>
#include <cstddef>
#include <unistd.h>
#include <signal.h>

#include <boost/exception/diagnostic_information.hpp>

namespace mcl = mir::client;
namespace md = mir::dispatch;
namespace mircv = mir::input::receiver;
namespace mev = mir::events;
namespace gp = google::protobuf;

namespace
{

class MirDisplayConfigurationStore
{
public:
    MirDisplayConfigurationStore(MirDisplayConfiguration* config)
        : config{config}
    {
    }

    ~MirDisplayConfigurationStore()
    {
        mcl::delete_config_storage(config);
    }

    MirDisplayConfiguration* operator->() const { return config; }

private:
    MirDisplayConfigurationStore(MirDisplayConfigurationStore const&) = delete;
    MirDisplayConfigurationStore& operator=(MirDisplayConfigurationStore const&) = delete;

    MirDisplayConfiguration* const config;
};

std::mutex connection_guard;
MirConnection* valid_connections{nullptr};
}

MirConnection::Deregisterer::~Deregisterer()
{
    std::lock_guard<std::mutex> lock(connection_guard);

    for (auto current = &valid_connections; *current; current = &(*current)->next_valid)
    {
        if (self == *current)
        {
            *current = self->next_valid;
            break;
        }
    }
}

MirConnection::MirConnection(std::string const& error_message) :
    deregisterer{this},
    channel(nullptr),
    server(nullptr),
    debug(nullptr),
    error_message(error_message)
{
}

MirConnection::MirConnection(
    mir::client::ConnectionConfiguration& conf) :
        deregisterer{this},
        logger(conf.the_logger()),
        connect_done{false},
        client_platform_factory(conf.the_client_platform_factory()),
        input_platform(conf.the_input_platform()),
        display_configuration(conf.the_display_configuration()),
        lifecycle_control(conf.the_lifecycle_control()),
        surface_map(conf.the_surface_map()),
        event_handler_register(conf.the_event_handler_register())
{
    connect_result.set_error("Protocol handshake incomplete");
    auto future_channel = conf.make_rpc_channel();
    std::promise<void> handshake_completion;
    handshake_done = handshake_completion.get_future();
    std::thread{[this](std::future<std::unique_ptr<google::protobuf::RpcChannel>>&& future_channel,
                       std::promise<void>&& handshake_completed)
    {
        decltype(channel) tmp_channel;
        try
        {
            tmp_channel = future_channel.get();
        }
        catch(std::future_error& err)
        {
            return;
        }
        catch(std::exception& err)
        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            connect_result.set_error(err.what());
            return;
        }

        std::lock_guard<decltype(mutex)> lock(mutex);
        server = std::make_unique<mir::protobuf::DisplayServer::Stub>(tmp_channel.get(), ::google::protobuf::Service::STUB_DOESNT_OWN_CHANNEL);
        debug = std::make_unique<mir::protobuf::Debug::Stub>(tmp_channel.get(), ::google::protobuf::Service::STUB_DOESNT_OWN_CHANNEL);
        eventloop = std::make_unique<md::SimpleDispatchThread>(std::dynamic_pointer_cast<md::Dispatchable>(tmp_channel));

        connect_result.set_error("connect not called");

        channel = std::move(tmp_channel);
        handshake_completed.set_value();
    }, std::move(future_channel), std::move(handshake_completion)}.detach();

    {
        std::lock_guard<std::mutex> lock(connection_guard);
        next_valid = valid_connections;
        valid_connections = this;
    }
}

MirConnection::~MirConnection() noexcept
{
    using namespace std::literals::chrono_literals;
    try
    {
        handshake_done.wait_for(1s);
    }
    catch(std::future_error&)
    {
        // Handshake is already done or errored out, great.
    }

    // We don't die while if are pending callbacks (as they touch this).
    // But, if after 500ms we don't get a call, assume it won't happen.
    connect_wait_handle.wait_for_pending(std::chrono::milliseconds(500));

    std::lock_guard<decltype(mutex)> lock(mutex);
    if (connect_result.has_platform())
    {
        auto const& platform = connect_result.platform();
        for (int i = 0, end = platform.fd_size(); i != end; ++i)
            close(platform.fd(i));
    }
}

MirWaitHandle* MirConnection::create_surface(
    MirSurfaceSpec const& spec,
    mir_surface_callback callback,
    void * context)
{
    auto surface = new MirSurface(this, *server, debug.get(), get_client_buffer_stream_factory(),
        input_platform, spec, callback, context);

    return surface->get_create_wait_handle();
}

char const * MirConnection::get_error_message()
{
    std::lock_guard<decltype(mutex)> lock(mutex);

    if (connect_result.has_error())
    {
        return connect_result.error().c_str();
    }

    return error_message.c_str();
}

void MirConnection::set_error_message(std::string const& error)
{
    error_message = error;
}


/* struct exists to work around google protobuf being able to bind
 "only 0, 1, or 2 arguments in the NewCallback function */
struct MirConnection::SurfaceRelease
{
    MirSurface * surface;
    MirWaitHandle * handle;
    mir_surface_callback callback;
    void * context;
};

void MirConnection::released(SurfaceRelease data)
{
    surface_map->erase(data.surface->id());

    // Erasing this surface from surface_map means that it will no longer receive events
    // If it's still focused, send an unfocused event before we kill it entirely
    if (data.surface->attrib(mir_surface_attrib_focus) == mir_surface_focused)
    {
        auto unfocus = mev::make_event(mir::frontend::SurfaceId{data.surface->id()}, mir_surface_attrib_focus, mir_surface_unfocused);
        data.surface->handle_event(*unfocus);
    }
    data.callback(data.surface, data.context);
    data.handle->result_received();
    delete data.surface;
}

MirWaitHandle* MirConnection::release_surface(
        MirSurface *surface,
        mir_surface_callback callback,
        void * context)
{
    auto new_wait_handle = new MirWaitHandle;

    SurfaceRelease surf_release{surface, new_wait_handle, callback, context};

    mir::protobuf::SurfaceId message;
    message.set_value(surface->id());

    {
        std::lock_guard<decltype(release_wait_handle_guard)> rel_lock(release_wait_handle_guard);
        release_wait_handles.push_back(new_wait_handle);
    }

    new_wait_handle->expect_result();
    server->release_surface(0, &message, &void_response,
                            gp::NewCallback(this, &MirConnection::released, surf_release));


    return new_wait_handle;
}

MirPromptSession* MirConnection::create_prompt_session()
{
    return new MirPromptSession(display_server(), event_handler_register);
}

void MirConnection::connected(mir_connected_callback callback, void * context)
{
    bool safe_to_callback = true;
    try
    {
        std::lock_guard<decltype(mutex)> lock(mutex);

        if (!connect_result.has_platform() || !connect_result.has_display_configuration())
        {
            if (!connect_result.has_error())
            {
                // We're handling an error scenario that means we're not sync'd
                // with the client code - a callback isn't safe (or needed)
                safe_to_callback = false;
                set_error_message("Connect failed");
            }
        }

        connect_done = true;

        /*
         * We need to create the client platform after the connection has been
         * established, to ensure that the client platform has access to all
         * needed data (e.g. platform package).
         */
        auto default_lifecycle_event_handler = [this](MirLifecycleState transition)
            {
                bool const expect_connection_lost = [&]
                    {
                        std::lock_guard<decltype(mutex)> lock(mutex);
                        return disconnecting;
                    }();

                if (transition == mir_lifecycle_connection_lost &&
                    !expect_connection_lost)
                {
                    /*
                     * We need to use kill() instead of raise() to ensure the signal
                     * is dispatched to the process even if it's blocked in the current
                     * thread.
                     */
                    kill(getpid(), SIGHUP);
                }
            };

        platform = client_platform_factory->create_client_platform(this);
        native_display = platform->create_egl_native_display();
        display_configuration->set_configuration(connect_result.display_configuration());
        lifecycle_control->set_lifecycle_event_handler(default_lifecycle_event_handler);
    }
    catch (std::exception const& e)
    {
        connect_result.set_error(std::string{"Failed to process connect response: "} +
                                 boost::diagnostic_information(e));
    }

    if (safe_to_callback) callback(this, context);
    connect_wait_handle.result_received();
}

MirWaitHandle* MirConnection::connect(
    const char* app_name,
    mir_connected_callback callback,
    void * context)
{
    {
        std::lock_guard<decltype(mutex)> lock(mutex);

        connect_parameters.set_application_name(app_name);
        connect_wait_handle.expect_result();
    }

    std::thread{[this, callback, context]()
    {
        try
        {
            handshake_done.get();
        }
        catch(std::future_error& err)
        {
            // Oops, handshaking was cancelled.
            return;
        }

        try
        {
            server->connect(
                        0,
                        &connect_parameters,
                        &connect_result,
                        google::protobuf::NewCallback(
                            this, &MirConnection::connected, callback, context));
        }
        catch (std::exception& err)
        {
            std::lock_guard<decltype(mutex)> lock(mutex);
            connect_result.set_error(std::string{"Error in connect()"} +
                                     boost::diagnostic_information(err));
        }
    }}.detach();

    return &connect_wait_handle;
}

void MirConnection::done_disconnect()
{
    /* todo: keeping all MirWaitHandles from a release surface until the end of the connection
       is a kludge until we have a better story about the lifetime of MirWaitHandles */
    {
        std::lock_guard<decltype(release_wait_handle_guard)> lock(release_wait_handle_guard);
        for (auto handle : release_wait_handles)
            delete handle;
    }

    // Ensure no racy lifecycle notifications can happen after disconnect completes
    lifecycle_control->set_lifecycle_event_handler([](MirLifecycleState){});
    disconnect_wait_handle.result_received();
}

MirWaitHandle* MirConnection::disconnect()
{
    {
        std::lock_guard<decltype(mutex)> lock(mutex);
        disconnecting = true;
    }
    disconnect_wait_handle.expect_result();
    server->disconnect(0, &ignored, &ignored,
                       google::protobuf::NewCallback(this, &MirConnection::done_disconnect));

    return &disconnect_wait_handle;
}

void MirConnection::done_platform_operation(
    mir_platform_operation_callback callback, void* context)
{
    auto reply = mir_platform_message_create(platform_operation_reply.opcode());

    set_error_message(platform_operation_reply.error());

    mir_platform_message_set_data(
        reply,
        platform_operation_reply.data().data(),
        platform_operation_reply.data().size());

    mir_platform_message_set_fds(
        reply,
        platform_operation_reply.fd().data(),
        platform_operation_reply.fd().size());

    callback(this, reply, context);

    platform_operation_wait_handle.result_received();
}

MirWaitHandle* MirConnection::platform_operation(
    MirPlatformMessage const* request,
    mir_platform_operation_callback callback, void* context)
{
    auto const client_response = platform->platform_operation(request);
    if (client_response)
    {
        set_error_message("");
        callback(this, client_response, context);
        return nullptr;
    }

    mir::protobuf::PlatformOperationMessage protobuf_request;

    protobuf_request.set_opcode(mir_platform_message_get_opcode(request));
    auto const request_data = mir_platform_message_get_data(request);
    auto const request_fds = mir_platform_message_get_fds(request);

    protobuf_request.set_data(request_data.data, request_data.size);
    for (size_t i = 0; i != request_fds.num_fds; ++i)
        protobuf_request.add_fd(request_fds.fds[i]);

    platform_operation_wait_handle.expect_result();
    server->platform_operation(
        0,
        &protobuf_request,
        &platform_operation_reply,
        google::protobuf::NewCallback(this, &MirConnection::done_platform_operation,
                                      callback, context));

    return &platform_operation_wait_handle;
}


bool MirConnection::is_valid(MirConnection *connection)
{
    {
        std::unique_lock<std::mutex> lock(connection_guard);
        for (auto current = valid_connections; current; current = current->next_valid)
        {
            if (connection == current)
            {
                lock.unlock();
                std::lock_guard<decltype(connection->mutex)> lock(connection->mutex);
                return !connection->connect_result.has_error();
            }
        }
    }
    return false;
}

void MirConnection::populate(MirPlatformPackage& platform_package)
{
    platform->populate(platform_package);
}

void MirConnection::populate_server_package(MirPlatformPackage& platform_package)
{
    // connect_result is write-once: once it's valid, we don't need to lock
    // to use it.
    if (connect_done && !connect_result.has_error() && connect_result.has_platform())
    {
        auto const& platform = connect_result.platform();

        platform_package.data_items = platform.data_size();
        for (int i = 0; i != platform.data_size(); ++i)
            platform_package.data[i] = platform.data(i);

        platform_package.fd_items = platform.fd_size();
        for (int i = 0; i != platform.fd_size(); ++i)
            platform_package.fd[i] = platform.fd(i);
    }
    else
    {
        platform_package.data_items = 0;
        platform_package.fd_items = 0;
    }
}

MirDisplayConfiguration* MirConnection::create_copy_of_display_config()
{
    std::lock_guard<decltype(mutex)> lock(mutex);
    return display_configuration->copy_to_client();
}

void MirConnection::available_surface_formats(
    MirPixelFormat* formats,
    unsigned int formats_size,
    unsigned int& valid_formats)
{
    valid_formats = 0;

    std::lock_guard<decltype(mutex)> lock(mutex);
    if (!connect_result.has_error())
    {
        valid_formats = std::min(
            static_cast<unsigned int>(connect_result.surface_pixel_format_size()),
            formats_size);

        for (auto i = 0u; i < valid_formats; i++)
        {
            formats[i] = static_cast<MirPixelFormat>(connect_result.surface_pixel_format(i));
        }
    }
}

std::shared_ptr<mir::client::ClientPlatform> MirConnection::get_client_platform()
{
    std::lock_guard<decltype(mutex)> lock(mutex);

    return platform;
}

std::shared_ptr<mir::client::ClientBufferStreamFactory> MirConnection::get_client_buffer_stream_factory()
{
    if (!buffer_stream_factory)
        buffer_stream_factory = std::make_shared<mcl::DefaultClientBufferStreamFactory>(platform, the_logger());
    return buffer_stream_factory;
}

EGLNativeDisplayType MirConnection::egl_native_display()
{
    std::lock_guard<decltype(mutex)> lock(mutex);

    return *native_display;
}

void MirConnection::on_surface_created(int id, MirSurface* surface)
{
    surface_map->insert(id, surface);
}

void MirConnection::register_lifecycle_event_callback(mir_lifecycle_event_callback callback, void* context)
{
    lifecycle_control->set_lifecycle_event_handler(std::bind(callback, this, std::placeholders::_1, context));
}

void MirConnection::register_display_change_callback(mir_display_config_callback callback, void* context)
{
    display_configuration->set_display_change_handler(std::bind(callback, this, context));
}

bool MirConnection::validate_user_display_config(MirDisplayConfiguration* config)
{
    MirDisplayConfigurationStore orig_config{display_configuration->copy_to_client()};

    if ((!config) || (config->num_outputs == 0) || (config->outputs == NULL) ||
        (config->num_outputs > orig_config->num_outputs))
    {
        return false;
    }

    for(auto i = 0u; i < config->num_outputs; i++)
    {
        auto const& output = config->outputs[i];
        auto const& orig_output = orig_config->outputs[i];

        if (output.output_id != orig_output.output_id)
            return false;

        if (output.connected && output.current_mode >= orig_output.num_modes)
            return false;
    }

    return true;
}

void MirConnection::done_display_configure()
{
    std::lock_guard<decltype(mutex)> lock(mutex);

    set_error_message(display_configuration_response.error());

    if (!display_configuration_response.has_error())
        display_configuration->set_configuration(display_configuration_response);

    return configure_display_wait_handle.result_received();
}

MirWaitHandle* MirConnection::configure_display(MirDisplayConfiguration* config)
{
    if (!validate_user_display_config(config))
    {
        return NULL;
    }

    mir::protobuf::DisplayConfiguration request;
    {
        std::lock_guard<decltype(mutex)> lock(mutex);

        for (auto i=0u; i < config->num_outputs; i++)
        {
            auto output = config->outputs[i];
            auto display_request = request.add_display_output();
            display_request->set_output_id(output.output_id);
            display_request->set_used(output.used);
            display_request->set_current_mode(output.current_mode);
            display_request->set_current_format(output.current_format);
            display_request->set_position_x(output.position_x);
            display_request->set_position_y(output.position_y);
            display_request->set_power_mode(output.power_mode);
            display_request->set_orientation(output.orientation);
        }
    }

    configure_display_wait_handle.expect_result();
    server->configure_display(0, &request, &display_configuration_response,
        google::protobuf::NewCallback(this, &MirConnection::done_display_configure));

    return &configure_display_wait_handle;
}

mir::protobuf::DisplayServer& MirConnection::display_server()
{
    return *server;
}

std::shared_ptr<mir::logging::Logger> const& MirConnection::the_logger() const
{
    return logger;
}
