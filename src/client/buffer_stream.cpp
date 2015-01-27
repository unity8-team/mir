/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#define MIR_LOG_COMPONENT "MirBufferStream"

#include "buffer_stream.h"

#include "mir/frontend/client_constants.h"

#include "mir/log.h"

#include "mir_toolkit/mir_native_buffer.h"
#include "mir/egl_native_window_factory.h"

#include "perf_report.h"
#include "logging/perf_report.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>

namespace mcl = mir::client;
namespace ml = mir::logging;
namespace mp = mir::protobuf;
namespace geom = mir::geometry;

namespace
{

void populate_buffer_package(
    MirBufferPackage& buffer_package,
    mir::protobuf::Buffer const& protobuf_buffer)
{
    if (!protobuf_buffer.has_error())
    {
        buffer_package.data_items = protobuf_buffer.data_size();
        for (int i = 0; i != protobuf_buffer.data_size(); ++i)
        {
            buffer_package.data[i] = protobuf_buffer.data(i);
        }

        buffer_package.fd_items = protobuf_buffer.fd_size();

        for (int i = 0; i != protobuf_buffer.fd_size(); ++i)
        {
            buffer_package.fd[i] = protobuf_buffer.fd(i);
        }

        buffer_package.stride = protobuf_buffer.stride();
        buffer_package.flags = protobuf_buffer.flags();
        buffer_package.width = protobuf_buffer.width();
        buffer_package.height = protobuf_buffer.height();
    }
    else
    {
        buffer_package.data_items = 0;
        buffer_package.fd_items = 0;
        buffer_package.stride = 0;
        buffer_package.flags = 0;
        buffer_package.width = 0;
        buffer_package.height = 0;
    }
}

std::shared_ptr<mcl::PerfReport>
make_perf_report(std::shared_ptr<ml::Logger> const& logger)
{
    // TODO: It seems strange that this directly uses getenv
    const char* report_target = getenv("MIR_CLIENT_PERF_REPORT");
    if (report_target && !strcmp(report_target, "log"))
    {
        return std::make_shared<mir::client::logging::PerfReport>(logger);
    }
    else
    {
        return std::make_shared<mir::client::NullPerfReport>();
    }
}
}

// TODO: It seems like a bit of a wart that we have to pass the Logger specifically here...perhaps
// due to the lack of an easily mockable client configuration interface (passing around
// connection can complicate unit tests ala MirSurface and test_client_mir_surface.cpp)
mcl::BufferStream::BufferStream(mp::DisplayServer& server,
    mcl::BufferStreamMode mode,
    std::shared_ptr<mcl::ClientBufferFactory> const& buffer_factory,
    std::shared_ptr<mcl::EGLNativeWindowFactory> const& native_window_factory,
    mp::BufferStream const& protobuf_bs,
    std::shared_ptr<ml::Logger> const& logger)
    : display_server(server),
      mode(mode),
      native_window_factory(native_window_factory),
      protobuf_bs(protobuf_bs),
      buffer_depository{buffer_factory, mir::frontend::client_buffer_cache_size},
      swap_interval_(1),
      perf_report(make_perf_report(logger))
      
{
    if (!protobuf_bs.has_id() || protobuf_bs.has_error())
        BOOST_THROW_EXCEPTION(std::runtime_error("Can not create buffer stream: " + std::string(protobuf_bs.error())));
    if (!protobuf_bs.has_buffer())
        BOOST_THROW_EXCEPTION(std::runtime_error("Buffer stream did not come with a buffer"));

    process_buffer(protobuf_bs.buffer());
    egl_native_window_ = native_window_factory->create_egl_native_window(this);

    perf_report->name_surface(std::to_string(protobuf_bs.id().value()).c_str());
}

mcl::BufferStream::~BufferStream()
{
}

void mcl::BufferStream::process_buffer(mp::Buffer const& buffer)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    
    auto buffer_package = std::make_shared<MirBufferPackage>();
    populate_buffer_package(*buffer_package, buffer);
    
    if (buffer.has_width() && buffer.has_height())
    {
        cached_buffer_size = geom::Size{buffer.width(), buffer.height()};
    }
    
    if (buffer.has_error())
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("BufferStream received buffer with error:" + buffer.error()));
    }

    try
    {
        auto pixel_format = static_cast<MirPixelFormat>(protobuf_bs.pixel_format());
        buffer_depository.deposit_package(buffer_package,
            buffer.buffer_id(),
            cached_buffer_size, pixel_format);
        perf_report->begin_frame(buffer.buffer_id());
    }
    catch (const std::runtime_error& err)
    {
        mir::log_error("Error processing incoming buffer " + std::string(err.what()));
    }
}

MirWaitHandle* mcl::BufferStream::next_buffer(std::function<void()> const& done)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    perf_report->end_frame(buffer_depository.current_buffer_id());

    secured_region.reset();

    mir::protobuf::BufferStreamId buffer_stream_id;
    buffer_stream_id.set_value(protobuf_bs.id().value());

// TODO: We can fix the strange "ID casting" used below in the second phase
// of buffer stream which generalizes and clarifies the server side logic.
    if (mode == mcl::BufferStreamMode::Producer)
    {
        mp::BufferRequest request;
        request.mutable_id()->set_value(protobuf_bs.id().value());
        request.mutable_buffer()->set_buffer_id(protobuf_bs.buffer().buffer_id());

        lock.unlock();
        next_buffer_wait_handle.expect_result();

        display_server.exchange_buffer(
            nullptr,
            &request,
            protobuf_bs.mutable_buffer(),
            google::protobuf::NewCallback(
            this, &mcl::BufferStream::next_buffer_received,
            done));
    }
    else
    {
        mp::ScreencastId screencast_id;
        screencast_id.set_value(protobuf_bs.id().value());

        lock.unlock();
        next_buffer_wait_handle.expect_result();

        display_server.screencast_buffer(
            nullptr,
            &screencast_id,
            protobuf_bs.mutable_buffer(),
            google::protobuf::NewCallback(
            this, &mcl::BufferStream::next_buffer_received,
            done));
    }


    return &next_buffer_wait_handle;
}

std::shared_ptr<mcl::ClientBuffer> mcl::BufferStream::get_current_buffer()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return buffer_depository.current_buffer();
}

EGLNativeWindowType mcl::BufferStream::egl_native_window()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return *egl_native_window_;
}

void mcl::BufferStream::release_cpu_region()
{
    secured_region.reset();
}

std::shared_ptr<mcl::MemoryRegion> mcl::BufferStream::secure_for_cpu_write()
{
    std::unique_lock<decltype(mutex)> lock(mutex);

    secured_region = buffer_depository.current_buffer()->secure_for_cpu_write();
    return secured_region;
}

void mcl::BufferStream::next_buffer_received(
    std::function<void()> done)                                             
{
    process_buffer(protobuf_bs.buffer());

    done();

    next_buffer_wait_handle.result_received();
}

/* mcl::EGLNativeSurface interface for EGLNativeWindow integration */
MirSurfaceParameters mcl::BufferStream::get_parameters() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return MirSurfaceParameters{
        "",
        cached_buffer_size.width.as_int(),
        cached_buffer_size.height.as_int(),
        static_cast<MirPixelFormat>(protobuf_bs.pixel_format()),
        static_cast<MirBufferUsage>(protobuf_bs.buffer_usage()),
        mir_display_output_id_invalid};
}

void mcl::BufferStream::request_and_wait_for_next_buffer()
{
    next_buffer([](){})->wait_for_all();
}

void mcl::BufferStream::on_configured()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    configure_wait_handle.result_received();
}

// TODO: It's a little strange that we use SurfaceAttrib here for the swap interval
// we take advantage of the confusion between buffer stream ids and surface ids...
// this should be resolved in the second phase of buffer stream which introduces the
// new server support.
void mcl::BufferStream::request_and_wait_for_configure(MirSurfaceAttrib attrib, int value)
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    if (attrib != mir_surface_attrib_swapinterval)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Attempt to configure surface attribute " + std::to_string(attrib) +
        " on BufferStream but only mir_surface_attrib_swapinterval is supported")); 
    }
    if (mode != mcl::BufferStreamMode::Producer)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Attempt to set swap interval on screencast is invalid"));
    }

    mp::SurfaceSetting setting, result;
    setting.mutable_surfaceid()->set_value(protobuf_bs.id().value());
    setting.set_attrib(attrib);
    setting.set_ivalue(value);
    lock.unlock();

    configure_wait_handle.expect_result();
    display_server.configure_surface(0, &setting, &result,
        google::protobuf::NewCallback(this, &mcl::BufferStream::on_configured));

    configure_wait_handle.wait_for_all();

    lock.lock();
    swap_interval_ = result.ivalue();
}

uint32_t mcl::BufferStream::get_current_buffer_id()
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return buffer_depository.current_buffer_id();
}

int mcl::BufferStream::swap_interval() const
{
    std::unique_lock<decltype(mutex)> lock(mutex);
    return swap_interval_;
}

void mcl::BufferStream::set_swap_interval(int interval)
{
    request_and_wait_for_configure(mir_surface_attrib_swapinterval, interval);
}
