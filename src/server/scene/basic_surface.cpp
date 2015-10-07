/*
 * Copyright © 2012-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by:
 *   Alan Griffiths <alan@octopull.co.uk>
 *   Thomas Voss <thomas.voss@canonical.com>
 */

#include "basic_surface.h"
#include "mir/compositor/buffer_stream.h"
#include "mir/frontend/event_sink.h"
#include "mir/input/input_channel.h"
#include "mir/shell/input_targeter.h"
#include "mir/input/input_sender.h"
#include "mir/graphics/buffer.h"
#include "mir/graphics/cursor_image.h"
#include "mir/geometry/displacement.h"

#include "mir/scene/scene_report.h"
#include "mir/scene/null_surface_observer.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <algorithm>

#include <string.h> // memcpy

namespace mc = mir::compositor;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace mf = mir::frontend;
namespace geom = mir::geometry;

void ms::SurfaceObservers::attrib_changed(MirSurfaceAttrib attrib, int value)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->attrib_changed(attrib, value); });
}

void ms::SurfaceObservers::resized_to(geometry::Size const& size)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->resized_to(size); });
}

void ms::SurfaceObservers::moved_to(geometry::Point const& top_left)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->moved_to(top_left); });
}

void ms::SurfaceObservers::hidden_set_to(bool hide)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->hidden_set_to(hide); });
}

void ms::SurfaceObservers::frame_posted(int frames_available)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->frame_posted(frames_available); });
}

void ms::SurfaceObservers::alpha_set_to(float alpha)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->alpha_set_to(alpha); });
}

void ms::SurfaceObservers::orientation_set_to(MirOrientation orientation)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->orientation_set_to(orientation); });
}

void ms::SurfaceObservers::transformation_set_to(glm::mat4 const& t)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->transformation_set_to(t); });
}

void ms::SurfaceObservers::cursor_image_set_to(mg::CursorImage const& image)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->cursor_image_set_to(image); });
}

void ms::SurfaceObservers::reception_mode_set_to(mi::InputReceptionMode mode)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->reception_mode_set_to(mode); });
}

void ms::SurfaceObservers::client_surface_close_requested()
{
    for_each([](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->client_surface_close_requested(); });
}

void ms::SurfaceObservers::keymap_changed(xkb_rule_names const& rules)
{
    for_each([&rules](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->keymap_changed(rules); });
}

void ms::SurfaceObservers::renamed(char const* name)
{
    for_each([name](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->renamed(name); });
}

struct ms::CursorStreamImageAdapter
{
    CursorStreamImageAdapter(ms::BasicSurface &surface)
        : surface(surface),
          observer{std::make_shared<FramePostObserver>(this)}
    {
    }

    ~CursorStreamImageAdapter()
    {
        reset();
    }

    void reset()
    {
        if (stream)
        {
            stream->remove_observer(observer);
            stream.reset();
        }
    }

    void update(std::shared_ptr<mf::BufferStream> const& new_stream, geom::Displacement const& new_hotspot)
    {
        if (new_stream == stream && new_hotspot == hotspot)
        {
            return;
        }
        else if (new_stream != stream)
        {
            if (stream) stream->remove_observer(observer);

            stream = std::dynamic_pointer_cast<mc::BufferStream>(new_stream);
            stream->add_observer(observer);
        }

        hotspot = new_hotspot;
        post_cursor_image_from_current_buffer();
    }

private:
    struct FramePostObserver : public ms::NullSurfaceObserver
    {
        FramePostObserver(CursorStreamImageAdapter const* self)
            : self(self)
        {
        }

        void frame_posted(int /* available */)
        {
            self->post_cursor_image_from_current_buffer();
        }

        CursorStreamImageAdapter const* const self;
    };

    void post_cursor_image_from_current_buffer() const
    {
        surface.set_cursor_from_buffer(*stream->lock_compositor_buffer(this), hotspot);
    }

    ms::BasicSurface& surface;
    std::shared_ptr<FramePostObserver> const observer;

    std::shared_ptr<mc::BufferStream> stream;
    geom::Displacement hotspot;
};

ms::BasicSurface::BasicSurface(
    std::string const& name,
    geometry::Rectangle rect,
    std::weak_ptr<Surface> const& parent,
    bool nonrectangular,
    std::shared_ptr<mc::BufferStream> const& buffer_stream,
    std::shared_ptr<mi::InputChannel> const& input_channel,
    std::shared_ptr<input::InputSender> const& input_sender,
    std::shared_ptr<mg::CursorImage> const& cursor_image,
    std::shared_ptr<SceneReport> const& report) :
    surface_name(name),
    surface_rect(rect),
    surface_alpha(1.0f),
    hidden(false),
    input_mode(mi::InputReceptionMode::normal),
    nonrectangular(nonrectangular),
    custom_input_rectangles(),
    surface_buffer_stream(buffer_stream),
    server_input_channel(input_channel),
    input_sender(input_sender),
    cursor_image_(cursor_image),
    report(report),
    parent_(parent),
    layers({StreamInfo{buffer_stream, {0,0}}}),
    cursor_stream_adapter{std::make_unique<ms::CursorStreamImageAdapter>(*this)},
    input_validator([this](MirEvent const& ev) { this->input_sender->send_event(ev, server_input_channel); })
{
    report->surface_created(this, surface_name);
}

ms::BasicSurface::BasicSurface(
    std::string const& name,
    geometry::Rectangle rect,
    bool nonrectangular,
    std::shared_ptr<mc::BufferStream> const& buffer_stream,
    std::shared_ptr<mi::InputChannel> const& input_channel,
    std::shared_ptr<input::InputSender> const& input_sender,
    std::shared_ptr<mg::CursorImage> const& cursor_image,
    std::shared_ptr<SceneReport> const& report) :
    BasicSurface(name, rect, std::shared_ptr<Surface>{nullptr}, nonrectangular,buffer_stream,
                 input_channel, input_sender, cursor_image, report)
{
}

ms::BasicSurface::~BasicSurface() noexcept
{
    report->surface_deleted(this, surface_name);
}

std::shared_ptr<mc::BufferStream> ms::BasicSurface::buffer_stream() const
{
    return surface_buffer_stream;
}

std::string ms::BasicSurface::name() const
{
    return surface_name;
}

void ms::BasicSurface::move_to(geometry::Point const& top_left)
{
    {
        std::unique_lock<std::mutex> lk(guard);
        surface_rect.top_left = top_left;
    }
    observers.moved_to(top_left);
}

float ms::BasicSurface::alpha() const
{
    std::unique_lock<std::mutex> lk(guard);
    return surface_alpha;
}

void ms::BasicSurface::set_hidden(bool hide)
{
    {
        std::unique_lock<std::mutex> lk(guard);
        hidden = hide;
    }
    observers.hidden_set_to(hide);
}

mir::geometry::Size ms::BasicSurface::size() const
{
    std::unique_lock<std::mutex> lk(guard);
    return surface_rect.size;
}

mir::geometry::Size ms::BasicSurface::client_size() const
{
    // TODO: In future when decorated, client_size() would be smaller than size
    return size();
}

std::shared_ptr<mf::BufferStream> ms::BasicSurface::primary_buffer_stream() const
{
    return surface_buffer_stream;
}

bool ms::BasicSurface::supports_input() const
{
    if (server_input_channel  && server_input_channel->client_fd() != -1)
        return true;
    return false;
}

int ms::BasicSurface::client_input_fd() const
{
    if (!supports_input())
        BOOST_THROW_EXCEPTION(std::logic_error("Surface does not support input"));
    return server_input_channel->client_fd();
}

std::shared_ptr<mi::InputChannel> ms::BasicSurface::input_channel() const
{
    return server_input_channel;
}

void ms::BasicSurface::set_input_region(std::vector<geom::Rectangle> const& input_rectangles)
{
    std::unique_lock<std::mutex> lock(guard);
    custom_input_rectangles = input_rectangles;
}

void ms::BasicSurface::resize(geom::Size const& desired_size)
{
    geom::Size new_size = desired_size;
    if (new_size.width <= geom::Width{0})   new_size.width = geom::Width{1};
    if (new_size.height <= geom::Height{0}) new_size.height = geom::Height{1};

    if (new_size == size())
        return;

    /*
     * Other combinations may still be invalid (like dimensions too big or
     * insufficient resources), but those are runtime and platform-specific, so
     * not predictable here. Such critical exceptions would arise from
     * the platform buffer allocator as a runtime_error via:
     */
    surface_buffer_stream->resize(new_size);

    // Now the buffer stream has successfully resized, update the state second;
    {
        std::unique_lock<std::mutex> lock(guard);
        surface_rect.size = new_size;
    }
    observers.resized_to(new_size);
}

geom::Point ms::BasicSurface::top_left() const
{
    std::unique_lock<std::mutex> lk(guard);
    return surface_rect.top_left;
}

geom::Rectangle ms::BasicSurface::input_bounds() const
{
    std::unique_lock<std::mutex> lk(guard);

    return surface_rect;
}

bool ms::BasicSurface::input_area_contains(geom::Point const& point) const
{
    std::unique_lock<std::mutex> lock(guard);

    if (!visible(lock))
        return false;

    // Restrict to bounding rectangle
    if (!surface_rect.contains(point))
        return false;

    // No custom input region means effective input region is whole surface
    if (custom_input_rectangles.empty())
        return true;

    // TODO: Perhaps creates some issues with transformation.
    auto local_point = geom::Point{0, 0} + (point-surface_rect.top_left);

    for (auto const& rectangle : custom_input_rectangles)
    {
        if (rectangle.contains(local_point))
        {
            return true;
        }
    }
    return false;
}

void ms::BasicSurface::set_alpha(float alpha)
{
    {
        std::unique_lock<std::mutex> lk(guard);
        surface_alpha = alpha;
    }
    observers.alpha_set_to(alpha);
}

void ms::BasicSurface::set_orientation(MirOrientation orientation)
{
    observers.orientation_set_to(orientation);
}

void ms::BasicSurface::set_transformation(glm::mat4 const& t)
{
    {
        std::unique_lock<std::mutex> lk(guard);
        transformation_matrix = t;
    }
    observers.transformation_set_to(t);
}

bool ms::BasicSurface::visible() const
{
    std::unique_lock<std::mutex> lk(guard);
    return visible(lk); 
}

bool ms::BasicSurface::visible(std::unique_lock<std::mutex>&) const
{
    bool visible{false};
    for (auto const& info : layers)
        visible |= info.stream->has_submitted_buffer();
    return !hidden && visible;
}

mi::InputReceptionMode ms::BasicSurface::reception_mode() const
{
    return input_mode;
}

void ms::BasicSurface::set_reception_mode(mi::InputReceptionMode mode)
{
    {
        std::lock_guard<std::mutex> lk(guard);
        input_mode = mode;
    }
    observers.reception_mode_set_to(mode);
}

MirSurfaceType ms::BasicSurface::type() const
{    
    std::unique_lock<std::mutex> lg(guard);
    return type_;
}

MirSurfaceType ms::BasicSurface::set_type(MirSurfaceType t)
{
    std::unique_lock<std::mutex> lg(guard);
    
    if (t < 0 || t > mir_surface_types)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface "
            "type."));
    }

    if (type_ != t)
    {
        type_ = t;
        lg.unlock();

        observers.attrib_changed(mir_surface_attrib_type, type_); 
    }

    return t;
}

MirSurfaceState ms::BasicSurface::state() const
{
    std::unique_lock<std::mutex> lg(guard);
    return state_;
}

MirSurfaceState ms::BasicSurface::set_state(MirSurfaceState s)
{
    if (s < mir_surface_state_unknown || s > mir_surface_states)
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface state."));

    std::unique_lock<std::mutex> lg(guard);
    if (state_ != s)
    {
        state_ = s;
        lg.unlock();
        observers.attrib_changed(mir_surface_attrib_state, s);
    }

    return s;
}

int ms::BasicSurface::set_swap_interval(int interval)
{
    if (interval < 0)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid swapinterval"));
    }

    std::unique_lock<std::mutex> lg(guard);
    if (swapinterval_ != interval)
    {
        swapinterval_ = interval;
        bool allow_dropping = (interval == 0);
        for (auto& info : layers) 
            info.stream->allow_framedropping(allow_dropping);

        lg.unlock();
        observers.attrib_changed(mir_surface_attrib_swapinterval, interval);
    }

    return interval;
}

MirSurfaceFocusState ms::BasicSurface::set_focus_state(MirSurfaceFocusState new_state)
{
    if (new_state != mir_surface_focused &&
        new_state != mir_surface_unfocused)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid focus state."));
    }

    std::unique_lock<std::mutex> lg(guard);
    if (focus_ != new_state)
    {
        focus_ = new_state;

        lg.unlock();
        observers.attrib_changed(mir_surface_attrib_focus, new_state);
    }

    return new_state;
}

MirOrientationMode ms::BasicSurface::set_preferred_orientation(MirOrientationMode new_orientation_mode)
{
    if ((new_orientation_mode & mir_orientation_mode_any) == 0)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid orientation mode"));
    }

    std::unique_lock<std::mutex> lg(guard);
    if (pref_orientation_mode != new_orientation_mode)
    {
        pref_orientation_mode = new_orientation_mode;
        lg.unlock();

        observers.attrib_changed(mir_surface_attrib_preferred_orientation, new_orientation_mode);
    }

    return new_orientation_mode;
}

int ms::BasicSurface::configure(MirSurfaceAttrib attrib, int value)
{
    int result = value;
    switch (attrib)
    {
    case mir_surface_attrib_type:
        result = set_type(static_cast<MirSurfaceType>(result));
        break;
    case mir_surface_attrib_state:
        result = set_state(static_cast<MirSurfaceState>(result));
        break;
    case mir_surface_attrib_focus:
        result = set_focus_state(static_cast<MirSurfaceFocusState>(result));
        break;
    case mir_surface_attrib_swapinterval:
        result = set_swap_interval(result);
        break;
    case mir_surface_attrib_dpi:
        result = set_dpi(result);
        break;
    case mir_surface_attrib_visibility:
        result = set_visibility(static_cast<MirSurfaceVisibility>(result));
        break;
    case mir_surface_attrib_preferred_orientation:
        result = set_preferred_orientation(static_cast<MirOrientationMode>(result));
        break;
    default:
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface attribute."));
    }

    return result;
}

int ms::BasicSurface::query(MirSurfaceAttrib attrib) const
{
    std::unique_lock<std::mutex> lg(guard);
    switch (attrib)
    {
        case mir_surface_attrib_type: return type_;
        case mir_surface_attrib_state: return state_;
        case mir_surface_attrib_swapinterval: return swapinterval_;
        case mir_surface_attrib_focus: return focus_;
        case mir_surface_attrib_dpi: return dpi_;
        case mir_surface_attrib_visibility: return visibility_;
        case mir_surface_attrib_preferred_orientation: return pref_orientation_mode;
        default: BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface "
                                                        "attribute."));
    }
}

void ms::BasicSurface::hide()
{
    set_hidden(true);
}

void ms::BasicSurface::show()
{
    set_hidden(false);
}

void ms::BasicSurface::set_cursor_image(std::shared_ptr<mg::CursorImage> const& image)
{
    {
        std::unique_lock<std::mutex> lock(guard);
        cursor_stream_adapter->reset();
        
        cursor_image_ = image;
    }

    observers.cursor_image_set_to(*image);
}    

std::shared_ptr<mg::CursorImage> ms::BasicSurface::cursor_image() const
{
    std::unique_lock<std::mutex> lock(guard);
    return cursor_image_;
}

namespace
{
struct CursorImageFromBuffer : public mg::CursorImage
{
    CursorImageFromBuffer(mg::Buffer &buffer, geom::Displacement const& hotspot)
        : buffer_size(buffer.size()),
          hotspot_(hotspot)
    {
        buffer.read([&](unsigned char const* buffer_pixels)
        {
            size_t buffer_size_bytes = buffer_size.width.as_int() * buffer_size.height.as_int()
                * MIR_BYTES_PER_PIXEL(buffer.pixel_format());
            pixels = std::unique_ptr<unsigned char[]>(
                new unsigned char[buffer_size_bytes]
            );
            memcpy(pixels.get(), buffer_pixels, buffer_size_bytes);
        });
    }
    void const* as_argb_8888() const
    {
        return pixels.get();
    }

    geom::Size size() const
    {
        return buffer_size;
    }

    geom::Displacement hotspot() const
    {
        return hotspot_;
    }

    geom::Size const buffer_size;
    geom::Displacement const hotspot_;

    std::unique_ptr<unsigned char[]> pixels;
};
}

void ms::BasicSurface::set_cursor_from_buffer(mg::Buffer& buffer, geom::Displacement const& hotspot)
{
    auto image = std::make_shared<CursorImageFromBuffer>(buffer, hotspot);
    {
        std::unique_lock<std::mutex> lock(guard);
        cursor_image_ = image;
    }
    observers.cursor_image_set_to(*image);
}

// In order to set the cursor image from a buffer stream, we use an adapter pattern,
// which observes buffers from the stream and copies them 1 by 1 to cursor images.
// We must be careful, when setting a new cursor image with ms::BasicSurface::set_cursor_image
// we need to reset the stream adapter (to halt the observation and allow the new static image
// to be set). Likewise from the adapter we must use set_cursor_from_buffer as
// opposed to the public set_cursor_from_image in order to avoid resetting the stream
// adapter.
void ms::BasicSurface::set_cursor_stream(std::shared_ptr<mf::BufferStream> const& stream,
                                         geom::Displacement const& hotspot)
{
    cursor_stream_adapter->update(stream, hotspot);
}

void ms::BasicSurface::request_client_surface_close()
{
    observers.client_surface_close_requested();
}

int ms::BasicSurface::dpi() const
{
    std::unique_lock<std::mutex> lock(guard);
    return dpi_;
}

int ms::BasicSurface::set_dpi(int new_dpi)
{
    if (new_dpi < 0)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid DPI value"));
    }

    std::unique_lock<std::mutex> lg(guard);
    if (dpi_ != new_dpi)
    {
        dpi_ = new_dpi;
        lg.unlock();
        observers.attrib_changed(mir_surface_attrib_dpi, new_dpi);
    }
    
    return new_dpi;
}

MirSurfaceVisibility ms::BasicSurface::set_visibility(MirSurfaceVisibility new_visibility)
{
    if (new_visibility != mir_surface_visibility_occluded &&
        new_visibility != mir_surface_visibility_exposed)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid visibility value"));
    }

    std::unique_lock<std::mutex> lg(guard);
    if (visibility_ != new_visibility)
    {
        visibility_ = new_visibility;
        lg.unlock();
        if (new_visibility == mir_surface_visibility_exposed)
        {
            for (auto& info : layers)
                info.stream->drop_old_buffers();
        }
        observers.attrib_changed(mir_surface_attrib_visibility, visibility_);
    }

    return new_visibility;
}

void ms::BasicSurface::add_observer(std::shared_ptr<SurfaceObserver> const& observer)
{
    observers.add(observer);
    for (auto& info : layers) 
        info.stream->add_observer(observer);
}

void ms::BasicSurface::remove_observer(std::weak_ptr<SurfaceObserver> const& observer)
{
    auto o = observer.lock();
    if (!o)
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid observer (previously destroyed)"));
    observers.remove(o);
    for (auto& info : layers) 
        info.stream->remove_observer(observer);
}

std::shared_ptr<ms::Surface> ms::BasicSurface::parent() const
{
    std::lock_guard<std::mutex> lg(guard);
    return parent_.lock();
}

namespace
{
//This class avoids locking for long periods of time by copying (or lazy-copying)
class SurfaceSnapshot : public mg::Renderable
{
public:
    SurfaceSnapshot(
        std::shared_ptr<mc::BufferStream> const& stream,
        void const* compositor_id,
        geom::Rectangle const& position,
        glm::mat4 const& transform,
        float alpha,
        bool shaped,
        mg::Renderable::ID id)
    : underlying_buffer_stream{stream},
      compositor_buffer{nullptr},
      compositor_id{compositor_id},
      alpha_{alpha},
      shaped_{shaped},
      screen_position_(position),
      transformation_(transform),
      id_(id)
    {
    }

    ~SurfaceSnapshot()
    {
    }
 
    std::shared_ptr<mg::Buffer> buffer() const override
    {
        if (!compositor_buffer)
            compositor_buffer = underlying_buffer_stream->lock_compositor_buffer(compositor_id);
        return compositor_buffer;
    }

    geom::Rectangle screen_position() const override
    { return screen_position_; }

    float alpha() const override
    { return alpha_; }

    glm::mat4 transformation() const override
    { return transformation_; }

    bool shaped() const override
    { return shaped_; }
 
    mg::Renderable::ID id() const override
    { return id_; }
private:
    std::shared_ptr<mc::BufferStream> const underlying_buffer_stream;
    std::shared_ptr<mg::Buffer> mutable compositor_buffer;
    void const*const compositor_id;
    float const alpha_;
    bool const shaped_;
    geom::Rectangle const screen_position_;
    glm::mat4 const transformation_;
    mg::Renderable::ID const id_; 
};
}

int ms::BasicSurface::buffers_ready_for_compositor(void const* id) const
{
    std::unique_lock<std::mutex> lk(guard);
    auto max_buf = 0;
    for (auto const& info : layers)
        max_buf = std::max(max_buf, info.stream->buffers_ready_for_compositor(id));
    return max_buf;
}

void ms::BasicSurface::consume(MirEvent const& event)
{
    input_validator.validate_and_dispatch(event);
}

void ms::BasicSurface::set_keymap(xkb_rule_names const& rules)
{
    observers.keymap_changed(rules);
}

void ms::BasicSurface::rename(std::string const& title)
{
    if (surface_name != title)
    {
        surface_name = title;
        observers.renamed(surface_name.c_str());
    }
}

void ms::BasicSurface::set_streams(std::list<scene::StreamInfo> const& s)
{
    {
        std::unique_lock<std::mutex> lk(guard);

        if (s.end() == std::find_if(s.begin(), s.end(),
            [this] (ms::StreamInfo const& info) { return info.stream == surface_buffer_stream; }))
        {
            BOOST_THROW_EXCEPTION(std::logic_error("cannot remove the created-with buffer stream yet"));
        }

        layers = s;
    }
    observers.moved_to(surface_rect.top_left);
}

mg::RenderableList ms::BasicSurface::generate_renderables(mc::CompositorID id) const
{
    std::unique_lock<std::mutex> lk(guard);
    mg::RenderableList list;
    for (auto const& info : layers)
    {
        if (info.stream->has_submitted_buffer())
        {
            list.emplace_back(std::make_shared<SurfaceSnapshot>(
                info.stream, id,
                geom::Rectangle{surface_rect.top_left + info.displacement, info.stream->stream_size()},
                transformation_matrix, surface_alpha, nonrectangular, info.stream.get()));
        }
    }
    return list;
}
