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
 */

#include "mir_buffer_stream.h"
#include "mir/frontend/client_constants.h"
#include "mir_toolkit/mir_native_buffer.h"
#include "egl_native_window_factory.h"

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

MirBufferStream::MirBufferStream(
    geom::Size const& size,
    MirPixelFormat pixel_format,
    MirBufferUsage buffer_usage,
    mir::protobuf::DisplayServer& server,
    std::shared_ptr<mcl::EGLNativeWindowFactory> const& egl_native_window_factory,
    std::shared_ptr<mcl::ClientBufferFactory> const& factory,
    mir_buffer_stream_callback callback, void* context)
    : size(size), pixel_format(pixel_format), buffer_usage(buffer_usage),
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
            this, &MirBufferStream::buffer_stream_created,
            callback, context));
}

MirWaitHandle* MirBufferStream::creation_wait_handle()
{
    return &create_buffer_stream_wait_handle;
}

bool MirBufferStream::valid()
{
    return !protobuf_buffer_stream.has_error();
}

MirSurfaceParameters MirBufferStream::get_parameters() const
{
    return MirSurfaceParameters{
        "",
        size.width.as_int(),
        size.height.as_int(),
        pixel_format,
        buffer_usage,
        mir_display_output_id_invalid};
}

std::shared_ptr<mcl::ClientBuffer> MirBufferStream::get_current_buffer()
{
    return buffer_depository.current_buffer();
}

MirWaitHandle* MirBufferStream::release(
        mir_buffer_stream_callback callback, void* context)
{
    mir::protobuf::BufferStreamId buffer_stream_id;
    buffer_stream_id.set_value(protobuf_buffer_stream.id().value());

    release_wait_handle.expect_result();
    server.release_buffer_stream(
        nullptr,
        &buffer_stream_id,
        &protobuf_void,
        google::protobuf::NewCallback(
            this, &MirBufferStream::released, callback, context));

    return &release_wait_handle;
}

MirWaitHandle* MirBufferStream::next_buffer(
    mir_buffer_stream_callback callback, void* context)
{
    // TODO: Update to be BufferStream ID and use cast in surface instead
    mir::protobuf::SurfaceId buffer_stream_id;
    buffer_stream_id.set_value(protobuf_buffer_stream.id().value());

    next_buffer_wait_handle.expect_result();
    server.next_buffer(
        nullptr,
        &buffer_stream_id,
        &protobuf_buffer,
        google::protobuf::NewCallback(
            this, &MirBufferStream::next_buffer_received,
            callback, context));

    return &next_buffer_wait_handle;
}

EGLNativeWindowType MirBufferStream::egl_native_window()
{
    return *egl_native_window_;
}

void MirBufferStream::request_and_wait_for_next_buffer()
{
    next_buffer(null_callback, nullptr)->wait_for_all();
}

void MirBufferStream::request_and_wait_for_configure(MirSurfaceAttrib, int)
{
}

void MirBufferStream::process_buffer(mir::protobuf::Buffer const& buffer)
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

void MirBufferStream::buffer_stream_created(
    mir_buffer_stream_callback callback, void* context)
{
    if (!protobuf_buffer_stream.has_error())
    {
        egl_native_window_ = egl_native_window_factory->create_egl_native_window(this);
        process_buffer(protobuf_buffer_stream.buffer());
    }

    callback(this, context);
    create_buffer_stream_wait_handle.result_received();
}

void MirBufferStream::released(
    mir_buffer_stream_callback callback, void* context)
{
    callback(this, context);
    release_wait_handle.result_received();
}

void MirBufferStream::next_buffer_received(
    mir_buffer_stream_callback callback, void* context)
{
    process_buffer(protobuf_buffer);

    callback(this, context);
    next_buffer_wait_handle.result_received();
}
