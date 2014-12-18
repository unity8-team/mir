/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 *              Robert Carr <robert.carr@canonical.com>
 */

#include "surfaceless_buffer_stream.h"
#include "mir_connection.h"
#include "egl_native_window_factory.h"
#include "client_buffer.h"

#include "mir/frontend/client_constants.h"

#include "mir_toolkit/mir_native_buffer.h"

#include <boost/throw_exception.hpp>

namespace mcl = mir::client;
namespace geom = mir::geometry;

namespace
{

void null_callback(MirBufferStream*, void*) {}

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

}

mcl::SurfacelessBufferStream::SurfacelessBufferStream(
    MirConnection *allocating_connection,
    geom::Size const& size,
    MirPixelFormat pixel_format,
    MirBufferUsage buffer_usage,
    mir::protobuf::DisplayServer& server,
    std::shared_ptr<mcl::EGLNativeWindowFactory> const& egl_native_window_factory,
    std::shared_ptr<mcl::ClientBufferFactory> const& factory,
    mir_buffer_stream_callback callback, void* context)
    : connection(allocating_connection), size(size), pixel_format(pixel_format), buffer_usage(buffer_usage),
      server(server), egl_native_window_factory{egl_native_window_factory},
      buffer_depository{factory, mir::frontend::client_buffer_cache_size}
{
    if (size.width.as_int()  == 0 ||
        size.height.as_int() == 0 ||
        pixel_format == mir_pixel_format_invalid)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid parameters"));
    }
    protobuf_buffer_stream.set_error("Not initialized");

    mir::protobuf::BufferStreamParameters parameters;

    parameters.set_width(size.width.as_uint32_t());
    parameters.set_height(size.height.as_uint32_t());
    parameters.set_pixel_format(pixel_format);
    parameters.set_buffer_usage(buffer_usage);

    create_buffer_stream_wait_handle.expect_result();
    server.create_buffer_stream(
        nullptr,
        &parameters,
        &protobuf_buffer_stream,
        google::protobuf::NewCallback(
            this, &mcl::SurfacelessBufferStream::buffer_stream_created,
            callback, context));
}

mcl::SurfacelessBufferStream::~SurfacelessBufferStream()
{
    release_cpu_region();
}

MirWaitHandle* mcl::SurfacelessBufferStream::creation_wait_handle()
{
    return &create_buffer_stream_wait_handle;
}

bool mcl::SurfacelessBufferStream::valid()
{
    std::lock_guard<std::mutex> lg(mutex);

    return !protobuf_buffer_stream.has_error();
}

MirSurfaceParameters mcl::SurfacelessBufferStream::get_parameters() const
{
    std::lock_guard<std::mutex> lg(mutex);

    return MirSurfaceParameters{
        "",
        size.width.as_int(),
        size.height.as_int(),
        pixel_format,
        buffer_usage,
        mir_display_output_id_invalid};
}

MirNativeBuffer* mcl::SurfacelessBufferStream::get_current_buffer_package()
{
    std::lock_guard<std::mutex> lg(mutex);

    auto platform = connection->get_client_platform();
    auto buffer = get_current_buffer();
    auto handle = buffer->native_buffer_handle();
    return platform->convert_native_buffer(handle.get());
}

std::shared_ptr<mcl::ClientBuffer> mcl::SurfacelessBufferStream::get_current_buffer()
{
    return buffer_depository.current_buffer();
}

MirWaitHandle* mcl::SurfacelessBufferStream::release(
        mir_buffer_stream_callback callback, void* context)
{
    std::lock_guard<std::mutex> lg(mutex);

    mir::protobuf::BufferStreamId buffer_stream_id;
    buffer_stream_id.set_value(protobuf_buffer_stream.id().value());

    release_wait_handle.expect_result();
    server.release_buffer_stream(
        nullptr,
        &buffer_stream_id,
        &protobuf_void,
        google::protobuf::NewCallback(
            this, &mcl::SurfacelessBufferStream::released, callback, context));

    return &release_wait_handle;
}

MirWaitHandle* mcl::SurfacelessBufferStream::next_buffer(
    mir_buffer_stream_callback callback, void* context)
{
    release_cpu_region();

    std::lock_guard<std::mutex> lg(mutex);

    mir::protobuf::BufferRequest request;
    request.mutable_id()->set_value(protobuf_buffer_stream.id().value());
    request.mutable_buffer()->set_buffer_id(buffer_depository.current_buffer_id());

    next_buffer_wait_handle.expect_result();
    server.exchange_buffer(
        nullptr,
        &request,
        &protobuf_buffer,
        google::protobuf::NewCallback(
            this, &mcl::SurfacelessBufferStream::next_buffer_received,
            callback, context));

    return &next_buffer_wait_handle;
}

EGLNativeWindowType mcl::SurfacelessBufferStream::egl_native_window()
{
    std::lock_guard<std::mutex> lg(mutex);

    return *egl_native_window_;
}

void mcl::SurfacelessBufferStream::request_and_wait_for_next_buffer()
{
    next_buffer(null_callback, nullptr)->wait_for_all();
}

void mcl::SurfacelessBufferStream::request_and_wait_for_configure(MirSurfaceAttrib, int)
{
}

void mcl::SurfacelessBufferStream::process_buffer(mir::protobuf::Buffer const& buffer)
{
    auto buffer_package = std::make_shared<MirBufferPackage>();
    populate_buffer_package(*buffer_package, buffer);

    try
    {
        buffer_depository.deposit_package(buffer_package,
                                          buffer.buffer_id(),
                                          size, pixel_format);
    }
    catch (const std::runtime_error& err)
    {
        // TODO: Report the error
    }
}

void mcl::SurfacelessBufferStream::buffer_stream_created(
    mir_buffer_stream_callback callback, void* context)
{

    {
    std::lock_guard<std::mutex> lg(mutex);

    if (!protobuf_buffer_stream.has_error())
    {
        pixel_format = static_cast<MirPixelFormat>(protobuf_buffer_stream.pixel_format());
        egl_native_window_ = egl_native_window_factory->create_egl_native_window(this);
        process_buffer(protobuf_buffer_stream.buffer());
    }
    }

    callback(this, context);
    create_buffer_stream_wait_handle.result_received();
}

void mcl::SurfacelessBufferStream::released(
    mir_buffer_stream_callback callback, void* context)
{
    callback(this, context);
    release_wait_handle.result_received();
}

void mcl::SurfacelessBufferStream::next_buffer_received(
    mir_buffer_stream_callback callback, void* context)
{
    process_buffer(protobuf_buffer);

    callback(this, context);
    next_buffer_wait_handle.result_received();
}

mir::protobuf::BufferStreamId mcl::SurfacelessBufferStream::protobuf_id() const
{
    return protobuf_buffer_stream.id();
}

void mcl::SurfacelessBufferStream::get_cpu_region(MirGraphicsRegion& region_out)
{
    auto buffer = buffer_depository.current_buffer();

    secured_region = buffer->secure_for_cpu_write();
    region_out.width = secured_region->width.as_uint32_t();
    region_out.height = secured_region->height.as_uint32_t();
    region_out.stride = secured_region->stride.as_uint32_t();
    region_out.pixel_format = secured_region->format;
    region_out.vaddr = secured_region->vaddr.get();
}

void mcl::SurfacelessBufferStream::release_cpu_region()
{
    std::lock_guard<std::mutex> lg(mutex);
    secured_region.reset();
}

MirPlatformType mcl::SurfacelessBufferStream::platform_type()
{
    auto platform = connection->get_client_platform();
    return platform->platform_type();
}
