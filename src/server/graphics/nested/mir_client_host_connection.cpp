/*
 * Copyright © 2014,2016 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir_client_host_connection.h"
#include "host_surface.h"
#include "host_stream.h"
#include "host_chain.h"
#include "host_surface_spec.h"
#include "native_buffer.h"
#include "mir_toolkit/mir_client_library.h"
#include "mir_toolkit/mir_buffer.h"
#include "mir_toolkit/mir_presentation_chain.h"
#include "mir/raii.h"
#include "mir/graphics/platform_operation_message.h"
#include "mir/graphics/cursor_image.h"
#include "mir/input/device.h"
#include "mir/input/device_capability.h"
#include "mir/input/pointer_configuration.h"
#include "mir/input/touchpad_configuration.h"
#include "mir/input/input_device_observer.h"
#include "mir/frontend/event_sink.h"
#include "mir/server_action_queue.h"

#include <boost/throw_exception.hpp>
#include <boost/exception/errinfo_errno.hpp>

#include <algorithm>
#include <stdexcept>

#include <cstring>

namespace geom = mir::geometry;
namespace mg = mir::graphics;
namespace mgn = mir::graphics::nested;
namespace mi = mir::input;
namespace mf = mir::frontend;

namespace
{
mgn::UniqueInputConfig make_input_config(MirConnection* con)
{
    return mgn::UniqueInputConfig(mir_connection_create_input_config(con), mir_input_config_destroy);
}

void display_config_callback_thunk(MirConnection* /*connection*/, void* context)
{
    (*static_cast<std::function<void()>*>(context))();
}

void platform_operation_callback(
    MirConnection*, MirPlatformMessage* reply, void* context)
{
    auto reply_ptr = static_cast<MirPlatformMessage**>(context);
    *reply_ptr = reply;
}

static void nested_lifecycle_event_callback_thunk(MirConnection* /*connection*/, MirLifecycleState state, void *context)
{
    msh::HostLifecycleEventListener* listener = static_cast<msh::HostLifecycleEventListener*>(context);
    listener->lifecycle_event_occurred(state);
}

MirPixelFormat const cursor_pixel_format = mir_pixel_format_argb_8888;

void copy_image(MirGraphicsRegion const& g, mg::CursorImage const& image)
{
    assert(g.pixel_format == cursor_pixel_format);

    auto const image_stride = image.size().width.as_int() * MIR_BYTES_PER_PIXEL(cursor_pixel_format);
    auto const image_height = image.size().height.as_int();

    auto dest = g.vaddr;
    auto src  = static_cast<char const*>(image.as_argb_8888());

    for (int row = 0; row != image_height; ++row)
    {
        std::memcpy(dest, src, image_stride);
        dest += g.stride;
        src += image_stride;
    }
}

class MirClientHostSurface : public mgn::HostSurface
{
public:
    MirClientHostSurface(
        MirConnection* mir_connection,
        MirSurfaceSpec* spec)
        : mir_connection(mir_connection),
          mir_surface{
              mir_surface_create_sync(spec)}
    {
        if (!mir_surface_is_valid(mir_surface))
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error(mir_surface_get_error_message(mir_surface)));
        }
    }

    ~MirClientHostSurface()
    {
        if (cursor) mir_buffer_stream_release_sync(cursor);
        mir_surface_release_sync(mir_surface);
    }

    void apply_spec(mgn::HostSurfaceSpec& spec) override
    {
        mir_surface_apply_spec(mir_surface, spec.handle());
    }

    EGLNativeWindowType egl_native_window() override
    {
        return reinterpret_cast<EGLNativeWindowType>(
            mir_buffer_stream_get_egl_native_window(mir_surface_get_buffer_stream(mir_surface)));
    }

    void set_event_handler(mir_surface_event_callback cb, void* context) override
    {
        mir_surface_set_event_handler(mir_surface, cb, context);
    }

    void set_cursor_image(mg::CursorImage const& image)
    {
        auto const image_width = image.size().width.as_int();
        auto const image_height = image.size().height.as_int();

        if ((image_width <= 0) || (image_height <= 0))
        {
            hide_cursor();
            return;
        }

        MirGraphicsRegion g;

        if (cursor)
        {
            mir_buffer_stream_get_graphics_region(cursor, &g);

            if (image_width != g.width || image_height != g.height)
            {
                mir_buffer_stream_release_sync(cursor);
                cursor = nullptr;
            }
        }

        bool const new_cursor{!cursor};

        if (new_cursor)
        {
            cursor = mir_connection_create_buffer_stream_sync(
                mir_connection,
                image_width,
                image_height,
                cursor_pixel_format,
                mir_buffer_usage_software);

            mir_buffer_stream_get_graphics_region(cursor, &g);
        }

        copy_image(g, image);

        mir_buffer_stream_swap_buffers_sync(cursor);

        if (new_cursor || cursor_hotspot != image.hotspot())
        {
            cursor_hotspot = image.hotspot();

            auto conf = mir_cursor_configuration_from_buffer_stream(
                cursor, cursor_hotspot.dx.as_int(), cursor_hotspot.dy.as_int());

            mir_surface_configure_cursor(mir_surface, conf);
            mir_cursor_configuration_destroy(conf);
        }
    }

    void hide_cursor()
    {
        if (cursor) { mir_buffer_stream_release_sync(cursor); cursor = nullptr; }

        auto conf = mir_cursor_configuration_from_name(mir_disabled_cursor_name);
        mir_surface_configure_cursor(mir_surface, conf);
        mir_cursor_configuration_destroy(conf);
    }

private:
    MirConnection* const mir_connection;
    MirSurface* const mir_surface;
    MirBufferStream* cursor{nullptr};
    mir::geometry::Displacement cursor_hotspot;
};

class MirClientHostStream : public mgn::HostStream
{
public:
    MirClientHostStream(MirConnection* connection, mg::BufferProperties const& properties) :
        stream(mir_connection_create_buffer_stream_sync(connection,
            properties.size.width.as_int(), properties.size.height.as_int(),
            properties.format,
            (properties.usage == mg::BufferUsage::hardware) ? mir_buffer_usage_hardware : mir_buffer_usage_software))
    {
    }

    ~MirClientHostStream()
    {
        mir_buffer_stream_release_sync(stream);
    }

    EGLNativeWindowType egl_native_window() const override
    {
        return reinterpret_cast<EGLNativeWindowType>(mir_buffer_stream_get_egl_native_window(stream));
    }

    MirBufferStream* handle() const override
    {
        return stream;
    }
private:
    MirBufferStream* const stream;
};

}

void const* mgn::MirClientHostConnection::NestedCursorImage::as_argb_8888() const
{
    return buffer.data();
}

mir::geometry::Size mgn::MirClientHostConnection::NestedCursorImage::size() const
{
    return size_;
}

mir::geometry::Displacement mgn::MirClientHostConnection::NestedCursorImage::hotspot() const
{
    return hotspot_;
}

mgn::MirClientHostConnection::NestedCursorImage::NestedCursorImage(mg::CursorImage const& other)
    : hotspot_(other.hotspot()),
      size_(other.size()),
      buffer(size_.width.as_int() * size_.height.as_int() * MIR_BYTES_PER_PIXEL(mir_pixel_format_argb_8888))
{
    std::memcpy(buffer.data(), other.as_argb_8888(), buffer.size());
}

mgn::MirClientHostConnection::NestedCursorImage& mgn::MirClientHostConnection::NestedCursorImage::operator=(mg::CursorImage const& other)
{
    hotspot_ = other.hotspot();
    size_ = other.size();
    buffer.resize(size_.width.as_int() * size_.height.as_int() * MIR_BYTES_PER_PIXEL(mir_pixel_format_argb_8888));
    std::memcpy(buffer.data(), other.as_argb_8888(), buffer.size());

    return *this;
}

mgn::MirClientHostConnection::MirClientHostConnection(
    std::string const& host_socket,
    std::string const& name,
    std::shared_ptr<msh::HostLifecycleEventListener> const& host_lifecycle_event_listener)
    : mir_connection{mir_connect_sync(host_socket.c_str(), name.c_str())},
      conf_change_callback{[]{}},
      host_lifecycle_event_listener{host_lifecycle_event_listener},
      event_callback{[](MirEvent const&, mir::geometry::Rectangle const&){}}
{
    if (!mir_connection_is_valid(mir_connection))
    {
        std::string const msg =
            "Nested Mir Platform Connection Error: " +
            std::string(mir_connection_get_error_message(mir_connection));

        BOOST_THROW_EXCEPTION(std::runtime_error(msg));
    }

    mir_connection_set_lifecycle_event_callback(
        mir_connection,
        nested_lifecycle_event_callback_thunk,
        std::static_pointer_cast<void>(host_lifecycle_event_listener).get());

    mir_connection_set_input_config_change_callback(
        mir_connection,
        [](MirConnection* connection, void* context)
        {
            auto obj = static_cast<MirClientHostConnection*>(context);
            if (obj->input_config_callback)
                obj->input_config_callback(make_input_config(connection));
        },
        this);
}

mgn::MirClientHostConnection::~MirClientHostConnection()
{
    mir_connection_release(mir_connection);
}

std::vector<int> mgn::MirClientHostConnection::platform_fd_items()
{
    MirPlatformPackage pkg;
    mir_connection_get_platform(mir_connection, &pkg);
    return std::vector<int>(pkg.fd, pkg.fd + pkg.fd_items);
}

EGLNativeDisplayType mgn::MirClientHostConnection::egl_native_display()
{
    return reinterpret_cast<EGLNativeDisplayType>(
        mir_connection_get_egl_native_display(mir_connection));
}

auto mgn::MirClientHostConnection::create_display_config()
    -> std::shared_ptr<MirDisplayConfiguration>
{
    return std::shared_ptr<MirDisplayConfiguration>(
        mir_connection_create_display_config(mir_connection),
        [] (MirDisplayConfiguration* c)
        {
            if (c) mir_display_config_destroy(c);
        });
}

void mgn::MirClientHostConnection::set_display_config_change_callback(
    std::function<void()> const& callback)
{
    mir_connection_set_display_config_change_callback(
        mir_connection,
        &display_config_callback_thunk,
        &(conf_change_callback = callback));
}

void mgn::MirClientHostConnection::apply_display_config(
    MirDisplayConfiguration& display_config)
{
    mir_wait_for(mir_connection_apply_display_config(mir_connection, &display_config));
}

std::shared_ptr<mgn::HostSurface> mgn::MirClientHostConnection::create_surface(
    std::shared_ptr<HostStream> const& stream,
    mir::geometry::Displacement displacement,
    mg::BufferProperties properties,
    char const* name, uint32_t output_id)
{
    std::lock_guard<std::mutex> lg(surfaces_mutex);
    auto spec = mir::raii::deleter_for(
        mir_connection_create_spec_for_normal_surface(
            mir_connection,
            properties.size.width.as_int(),
            properties.size.height.as_int(),
            properties.format),
        mir_surface_spec_release);

    MirBufferUsage usage = (properties.usage == mg::BufferUsage::hardware) ?
        mir_buffer_usage_hardware : mir_buffer_usage_software; 

    mir_surface_spec_set_name(spec.get(), name);
    mir_surface_spec_set_buffer_usage(spec.get(), usage);
    mir_surface_spec_set_fullscreen_on_output(spec.get(), output_id);
    MirBufferStreamInfo info { stream->handle(), displacement.dx.as_int(), displacement.dy.as_int() };
    mir_surface_spec_set_streams(spec.get(), &info, 1);

    auto surf = std::shared_ptr<MirClientHostSurface>(
        new MirClientHostSurface(mir_connection, spec.get()),
        [this](MirClientHostSurface *surf)
        {
            std::lock_guard<std::mutex> lg(surfaces_mutex);
            auto it = std::find(surfaces.begin(), surfaces.end(), surf);
            surfaces.erase(it);
            delete surf;
        });

    if (stored_cursor_image.size().width.as_int() * stored_cursor_image.size().height.as_int())
        surf->set_cursor_image(stored_cursor_image);

    surfaces.push_back(surf.get());
    return surf;
}

mg::PlatformOperationMessage mgn::MirClientHostConnection::platform_operation(
    unsigned int op, mg::PlatformOperationMessage const& request)
{
    auto const msg = mir::raii::deleter_for(
        mir_platform_message_create(op),
        mir_platform_message_release);

    mir_platform_message_set_data(msg.get(), request.data.data(), request.data.size());
    mir_platform_message_set_fds(msg.get(), request.fds.data(), request.fds.size());

    MirPlatformMessage* raw_reply{nullptr};

    auto const wh = mir_connection_platform_operation(
        mir_connection, msg.get(), platform_operation_callback, &raw_reply);
    mir_wait_for(wh);

    auto const reply = mir::raii::deleter_for(
        raw_reply,
        mir_platform_message_release);

    auto reply_data = mir_platform_message_get_data(reply.get());
    auto reply_fds = mir_platform_message_get_fds(reply.get());

    return PlatformOperationMessage{
        {static_cast<uint8_t const*>(reply_data.data),
         static_cast<uint8_t const*>(reply_data.data) + reply_data.size},
        {reply_fds.fds, reply_fds.fds + reply_fds.num_fds}};
}

void mgn::MirClientHostConnection::set_cursor_image(mg::CursorImage const& image)
{
    std::lock_guard<std::mutex> lg(surfaces_mutex);
    stored_cursor_image = image;
    for (auto s : surfaces)
    {
        auto surface = static_cast<MirClientHostSurface*>(s);
        surface->set_cursor_image(stored_cursor_image);
    }
}

void mgn::MirClientHostConnection::hide_cursor()
{
    std::lock_guard<std::mutex> lg(surfaces_mutex);
    for (auto s : surfaces)
    {
        auto surface = static_cast<MirClientHostSurface*>(s);
        surface->hide_cursor();
    }
}

auto mgn::MirClientHostConnection::graphics_platform_library() -> std::string
{
    MirModuleProperties properties = { nullptr, 0, 0, 0, nullptr };

    mir_connection_get_graphics_module(mir_connection, &properties);

    if (properties.filename == nullptr)
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Cannot identify host graphics platform"));
    }

    return properties.filename;
}

mgn::UniqueInputConfig mgn::MirClientHostConnection::create_input_device_config()
{
    return make_input_config(mir_connection);
}

void mgn::MirClientHostConnection::set_input_device_change_callback(std::function<void(UniqueInputConfig)> const& cb)
{
    input_config_callback = cb;
}

void mgn::MirClientHostConnection::set_input_event_callback(std::function<void(MirEvent const&, mir::geometry::Rectangle const&)> const& cb)
{
    event_callback = cb;
}

void mgn::MirClientHostConnection::emit_input_event(MirEvent const& event, mir::geometry::Rectangle const& source_frame)
{
    event_callback(event, source_frame);
}

std::unique_ptr<mgn::HostStream> mgn::MirClientHostConnection::create_stream(
    mg::BufferProperties const& properties) const
{
    return std::make_unique<MirClientHostStream>(mir_connection, properties);
}


struct Chain : mgn::HostChain
{
    Chain(MirConnection* con) :
        chain(mir_connection_create_presentation_chain_sync(con))
    {
    }
    ~Chain()
    {
        mir_presentation_chain_release(chain);
    }

    void submit_buffer(mgn::NativeBuffer& buffer)
    {
        if (buffer.client_handle() != current_buf)
        {
            printf("PUMP NEW IN\n");
            current_buf = buffer.client_handle();
            mir_presentation_chain_submit_buffer(chain, buffer.client_handle());
        }
    }

    MirPresentationChain* handle()
    {
        return chain;
    }
private:
    MirBuffer* current_buf = nullptr;
    MirPresentationChain* chain;
};

std::unique_ptr<mgn::HostChain> mgn::MirClientHostConnection::create_chain() const
{
    return std::make_unique<Chain>(mir_connection);




}

namespace
{
void bbuffer_created(MirBuffer* buffer, void* context);
struct XNativeBuffer : mgn::NativeBuffer
{
    XNativeBuffer(MirBuffer* b) :
        b(b),
        mine(true)
    {
        mir_buffer_set_callback(b, bbuffer_created, this);
    }
    ~XNativeBuffer() { printf("DESTROY BE\n"); mir_buffer_release(b); }

    void sync(MirBufferAccess access, std::chrono::nanoseconds ns) override
    {
        mir_buffer_wait_for_access(b, access, ns.count());
    }
    MirBuffer* client_handle() const override
    {
        return b;
    }
    MirNativeBuffer* get_native_handle() override
    {
        return mir_buffer_get_native_buffer(b, mir_read_write);
    }
    MirGraphicsRegion get_graphics_region() override
    {
        return mir_buffer_get_graphics_region(b, mir_read_write);
    }
    geom::Size size() const override
    {
        return {mir_buffer_get_width(b), mir_buffer_get_height(b) };
    }
    MirPixelFormat format() const override
    {
        return mir_buffer_get_pixel_format(b);
    }

    void avail()
    {
        mine = true;
        if (f)
        {
            printf("FN!\n");
            f();
        }
    }

    void on_ownership_notification(std::function<void()> const& fn)
    {
        f = fn;
    }

    MirBufferPackage* package() const
    {
       return mir_buffer_get_buffer_package(b);
    }

    void set_fence(mir::Fd)
    {
    }

    mir::Fd fence() const
    {
        return mir::Fd(mir::Fd::invalid);
    }

    std::function<void()> f;
    MirBuffer*b;
    bool mine;
};
void bbuffer_created(MirBuffer*, void* context)
{
    auto xn = static_cast<XNativeBuffer*>(context);
    xn->avail();
}
}

namespace
{
void abuffer_created(MirBuffer* buffer, void* context)
{
    printf("INCOMING\n");
    auto connection = static_cast<mgn::MirClientHostConnection::BufferCreation*>(context);
    connection->connection->buffer_created(buffer, connection);
}
}

void mgn::MirClientHostConnection::buffer_created(MirBuffer* b, BufferCreation* c)
{
    printf("INCOMING BUFFER\n");
    std::unique_lock<std::mutex> lk(c->mut);
    c->b = b;
    c->cv.notify_all();
}

std::shared_ptr<mgn::NativeBuffer> mgn::MirClientHostConnection::create_buffer(
    mg::BufferProperties const& properties)
{
    c.push_back(std::make_shared<BufferCreation>(this, count++));
    auto r = c.back().get(); 

    mir_connection_allocate_buffer(
        mir_connection,
        properties.size.width.as_int(),
        properties.size.height.as_int(),
        properties.format,
        mir_buffer_usage_hardware,
        abuffer_created, r);

    std::unique_lock<std::mutex> lk(r->mut);
    r->cv.wait(lk, [&]{ return r->b; });
    if (!mir_buffer_is_valid(r->b))
        BOOST_THROW_EXCEPTION(std::runtime_error("could not allocate MirBuffer"));

    printf("MAKE BE %X\n", (int)(long) r->b);
    auto ar = std::make_shared<XNativeBuffer>(r->b);
    for(auto it = c.begin(); it != c.end();)
    {
        if (r->count == (*it)->count)
            it = c.erase(it);
        else
            it++;
    }

    return ar;
}

std::unique_ptr<mgn::HostSurfaceSpec> mgn::MirClientHostConnection::create_surface_spec()
{
    return std::make_unique<mgn::SurfaceSpec>(mir_connection);
}
