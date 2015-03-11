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
#include "mir/geometry/displacement.h"

#include "mir/scene/scene_report.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <algorithm>

namespace mc = mir::compositor;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

void ms::SurfaceObservers::surface_changed(Surface const& s, Change c)
{
    for_each([&](std::shared_ptr<SurfaceObserver> const& observer)
        { observer->surface_changed(s, c); });
}

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
    first_frame_posted(false),
    hidden(false),
    input_mode(mi::InputReceptionMode::normal),
    nonrectangular(nonrectangular),
    custom_input_rectangles(),
    surface_buffer_stream(buffer_stream),
    server_input_channel(input_channel),
    input_sender(input_sender),
    cursor_image_(cursor_image),
    report(report),
    parent_(parent)
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

void ms::BasicSurface::force_requests_to_complete()
{
    surface_buffer_stream->force_requests_to_complete();
}

ms::BasicSurface::~BasicSurface() noexcept
{
    report->surface_deleted(this, surface_name);

    if (surface_buffer_stream) // some tests use null for surface_buffer_stream
        surface_buffer_stream->drop_client_requests();
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
    observers.surface_changed(*this, SurfaceObserver::position);
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
    observers.surface_changed(*this, SurfaceObserver::visibility);
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

MirPixelFormat ms::BasicSurface::pixel_format() const
{
    return surface_buffer_stream->get_stream_pixel_format();
}

void ms::BasicSurface::swap_buffers(mg::Buffer* old_buffer, std::function<void(mg::Buffer* new_buffer)> complete)
{
    if (old_buffer)
    {
        surface_buffer_stream->release_client_buffer(old_buffer);
        {
            std::unique_lock<std::mutex> lk(guard);
            first_frame_posted = true;
        }

        observers.surface_changed(*this, SurfaceObserver::content);
    }

    surface_buffer_stream->acquire_client_buffer(complete);
}

void ms::BasicSurface::allow_framedropping(bool allow)
{
    surface_buffer_stream->allow_framedropping(allow);
}

std::shared_ptr<mg::Buffer> ms::BasicSurface::snapshot_buffer() const
{
    return surface_buffer_stream->lock_snapshot_buffer();
}

bool ms::BasicSurface::supports_input() const
{
    if (server_input_channel)
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
    observers.surface_changed(*this, SurfaceObserver::size);
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
    observers.surface_changed(*this, SurfaceObserver::opacity);
}

void ms::BasicSurface::set_orientation(MirOrientation o)
{
    orientation_ = o;
    observers.surface_changed(*this, SurfaceObserver::orientation);
}

MirOrientation ms::BasicSurface::orientation() const
{
    std::unique_lock<std::mutex> lk(guard);
    return orientation_;
}

void ms::BasicSurface::set_transformation(glm::mat4 const& t)
{
    {
        std::unique_lock<std::mutex> lk(guard);
        transformation_matrix = t;
    }
    observers.surface_changed(*this, SurfaceObserver::transformation);
}

bool ms::BasicSurface::visible() const
{
    std::unique_lock<std::mutex> lk(guard);
    return visible(lk); 
}

bool ms::BasicSurface::visible(std::unique_lock<std::mutex>&) const
{
    return !hidden && first_frame_posted;
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
    observers.surface_changed(*this, SurfaceObserver::input_mode);
}

void ms::BasicSurface::with_most_recent_buffer_do(
    std::function<void(mg::Buffer&)> const& exec)
{
    auto buf = snapshot_buffer();
    exec(*buf);
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

        observers.surface_changed(*this, SurfaceObserver::type);
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
        set_hidden(s == mir_surface_state_hidden);
        
        observers.surface_changed(*this, SurfaceObserver::state);
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
        allow_framedropping(allow_dropping);

        lg.unlock();
        observers.surface_changed(*this, SurfaceObserver::swapinterval);
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
        observers.surface_changed(*this, SurfaceObserver::focus);
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

        observers.surface_changed(*this, SurfaceObserver::pref_orientation);
    }

    return new_orientation_mode;
}

void ms::BasicSurface::take_input_focus(std::shared_ptr<msh::InputTargeter> const& targeter)
{
    targeter->focus_changed(input_channel());
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
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid surface "
                                               "attribute."));
        break;
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
        cursor_image_ = image;
    }

    observers.surface_changed(*this, SurfaceObserver::cursor);
}
    

std::shared_ptr<mg::CursorImage> ms::BasicSurface::cursor_image() const
{
    std::unique_lock<std::mutex> lock(guard);
    return cursor_image_;
}

void ms::BasicSurface::request_client_surface_close()
{
    observers.surface_changed(*this, SurfaceObserver::close);
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
        observers.surface_changed(*this, SurfaceObserver::dpi);
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
        lg.unlock();
        if (new_visibility == mir_surface_visibility_exposed)
            surface_buffer_stream->drop_old_buffers();
        observers.surface_changed(*this, SurfaceObserver::visibility);
    }

    return new_visibility;
}

void ms::BasicSurface::add_observer(std::shared_ptr<SurfaceObserver> const& observer)
{
    observers.add(observer);
}

void ms::BasicSurface::remove_observer(std::weak_ptr<SurfaceObserver> const& observer)
{
    auto o = observer.lock();
    if (!o)
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid observer (previously destroyed)"));
    observers.remove(o);
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

std::unique_ptr<mg::Renderable> ms::BasicSurface::compositor_snapshot(void const* compositor_id) const
{
    std::unique_lock<std::mutex> lk(guard);

    return std::make_unique<SurfaceSnapshot>(
        surface_buffer_stream,
        compositor_id,
        surface_rect,
        transformation_matrix,
        surface_alpha,
        nonrectangular,
        this);
}

int ms::BasicSurface::buffers_ready_for_compositor(void const* id) const
{
    std::unique_lock<std::mutex> lk(guard);
    return surface_buffer_stream->buffers_ready_for_compositor(id);
}

void ms::BasicSurface::consume(MirEvent const& event)
{
    input_sender->send_event(event, server_input_channel);
}

void ms::BasicSurface::set_keymap(xkb_rule_names const&)
{
    observers.surface_changed(*this, SurfaceObserver::keymap);
}
