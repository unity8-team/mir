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

#include "surface_stack.h"
#include "rendering_tracker.h"
#include "mir/scene/surface.h"
#include "mir/scene/scene_report.h"
#include "mir/compositor/scene_element.h"
#include "mir/graphics/renderable.h"

#include <boost/throw_exception.hpp>

#include <algorithm>
#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>

namespace ms = mir::scene;
namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace geom = mir::geometry;

namespace
{

class SurfaceSceneElement : public mc::SceneElement
{
public:
    SurfaceSceneElement(
        std::shared_ptr<ms::Surface> surface,
        std::shared_ptr<ms::RenderingTracker> const& tracker,
        mc::CompositorID id)
        : renderable_{surface->compositor_snapshot(id)},
          tracker{tracker},
          cid{id},
          decor(mc::Decoration::Type::surface, surface->name())
    {
    }

    std::shared_ptr<mg::Renderable> renderable() const override
    {
        return renderable_;
    }

    void rendered() override
    {
        tracker->rendered_in(cid);
    }

    void occluded() override
    {
        tracker->occluded_in(cid);
    }

    mc::Decoration const& decoration() const override
    {
        return decor;
    }

private:
    std::shared_ptr<mg::Renderable> const renderable_;
    std::shared_ptr<ms::RenderingTracker> const tracker;
    mc::CompositorID cid;
    mc::Decoration const decor;
};

//note: something different than a 2D/HWC overlay
class OverlaySceneElement : public mc::SceneElement
{
public:
    OverlaySceneElement(
        std::shared_ptr<mg::Renderable> renderable)
        : renderable_{renderable}
    {
    }

    std::shared_ptr<mg::Renderable> renderable() const override
    {
        return renderable_;
    }

    void rendered() override
    {
    }

    void occluded() override
    {
    }

private:
    std::shared_ptr<mg::Renderable> const renderable_;
};

}

ms::SurfaceStack::SurfaceStack(
    std::shared_ptr<SceneReport> const& report) :
    report{report}
{
}

mc::SceneElementSequence ms::SurfaceStack::scene_elements_for(mc::CompositorID id)
{
    std::lock_guard<decltype(guard)> lg(guard);

    scene_changed = false;
    mc::SceneElementSequence elements;
    for (auto const& layer : layers_by_depth)
    {
        for (auto const& surface : layer.second) 
        {
            if (surface->visible())
            {
                auto element = std::make_shared<SurfaceSceneElement>(
                    surface,
                    rendering_trackers[surface.get()],
                    id);
                elements.emplace_back(element);
            }
        }
    }
    for (auto const& renderable : overlays)
    {
        elements.emplace_back(std::make_shared<OverlaySceneElement>(renderable));
    }
    return elements;
}

int ms::SurfaceStack::frames_pending(mc::CompositorID id) const
{
    std::lock_guard<decltype(guard)> lg(guard);

    int result = scene_changed ? 1 : 0;
    for (auto const& layer : layers_by_depth)
    {
        for (auto const& surface : layer.second) 
        {
            // TODO: Rename mir_surface_attrib_visibility as it's obviously
            //       confusing with visible()
            if (surface->visible() &&
                surface->query(mir_surface_attrib_visibility) ==
                    mir_surface_visibility_exposed)
            {
                // Note that we ask the surface and not a Renderable.
                // This is because we don't want to waste time and resources
                // on a snapshot till we're sure we need it...
                int ready = surface->buffers_ready_for_compositor(id);
                if (ready > result)
                    result = ready;
            }
        }
    }
    return result;
}

void ms::SurfaceStack::register_compositor(mc::CompositorID cid)
{
    std::lock_guard<decltype(guard)> lg(guard);

    registered_compositors.insert(cid);

    update_rendering_tracker_compositors();
}

void ms::SurfaceStack::unregister_compositor(mc::CompositorID cid)
{
    std::lock_guard<decltype(guard)> lg(guard);

    registered_compositors.erase(cid);

    update_rendering_tracker_compositors();
}

void ms::SurfaceStack::add_input_visualization(
    std::shared_ptr<mg::Renderable> const& overlay)
{
    {
        std::lock_guard<decltype(guard)> lg(guard);
        overlays.push_back(overlay);
    }
    emit_scene_changed();
}

void ms::SurfaceStack::remove_input_visualization(
    std::weak_ptr<mg::Renderable> const& weak_overlay)
{
    auto overlay = weak_overlay.lock();
    {
        std::lock_guard<decltype(guard)> lg(guard);
        auto const p = std::find(overlays.begin(), overlays.end(), overlay);
        if (p == overlays.end())
        {
            BOOST_THROW_EXCEPTION(std::runtime_error("Attempt to remove an overlay which was never added or which has been previously removed"));
        }
        overlays.erase(p);
    }
    
    emit_scene_changed();
}

void ms::SurfaceStack::emit_scene_changed()
{
    {
        std::lock_guard<decltype(guard)> lg(guard);
        scene_changed = true;
    }
    observers.scene_changed();
}

void ms::SurfaceStack::add_surface(
    std::shared_ptr<Surface> const& surface,
    DepthId depth,
    mi::InputReceptionMode input_mode)
{
    {
        std::lock_guard<decltype(guard)> lg(guard);
        layers_by_depth[depth].push_back(surface);
        create_rendering_tracker_for(surface);
    }
    surface->set_reception_mode(input_mode);
    observers.surface_added(surface.get());

    report->surface_added(surface.get(), surface.get()->name());
}

void ms::SurfaceStack::remove_surface(std::weak_ptr<Surface> const& surface)
{
    auto const keep_alive = surface.lock();

    bool found_surface = false;
    {
        std::lock_guard<decltype(guard)> lg(guard);

        for (auto &layer : layers_by_depth)
        {
            auto &surfaces = layer.second;
            auto const p = std::find(surfaces.begin(), surfaces.end(), keep_alive);

            if (p != surfaces.end())
            {
                surfaces.erase(p);
                rendering_trackers.erase(keep_alive.get());
                found_surface = true;
                break;
            }
        }
    }

    if (found_surface)
    {
        observers.surface_removed(keep_alive.get());

        report->surface_removed(keep_alive.get(), keep_alive.get()->name());
    }
    // TODO: error logging when surface not found
}

namespace
{
template <typename Container>
struct InReverse {
    Container& container;
    auto begin() -> decltype(container.rbegin()) { return container.rbegin(); }
    auto end() -> decltype(container.rend()) { return container.rend(); }
};

template <typename Container>
InReverse<Container> in_reverse(Container& container) { return InReverse<Container>{container}; }
}

auto ms::SurfaceStack::surface_at(geometry::Point cursor) const
-> std::shared_ptr<Surface>
{
    std::lock_guard<decltype(guard)> lg(guard);
    for (auto &layer : in_reverse(layers_by_depth))
    {
        for (auto const& surface : in_reverse(layer.second))
        {
            // TODO There's a lack of clarity about how the input area will
            // TODO be maintained and whether this test will detect clicks on
            // TODO decorations (it should) as these may be outside the area
            // TODO known to the client.  But it works for now.
            if (surface->input_area_contains(cursor))
                    return surface;
        }
    }

    return {};
}

void ms::SurfaceStack::for_each(std::function<void(std::shared_ptr<mi::Surface> const&)> const& callback)
{
    std::lock_guard<decltype(guard)> lg(guard);
    for (auto &layer : layers_by_depth)
    {
        for (auto it = layer.second.begin(); it != layer.second.end(); ++it)
        {
            if ((*it)->query(mir_surface_attrib_visibility) ==
                MirSurfaceVisibility::mir_surface_visibility_exposed)
            {
                callback(*it);
            }
        }
    }
}

void ms::SurfaceStack::raise(std::weak_ptr<Surface> const& s)
{
    auto surface = s.lock();

    {
        std::unique_lock<decltype(guard)> ul(guard);
        for (auto &layer : layers_by_depth)
        {
            auto &surfaces = layer.second;
            auto const p = std::find(surfaces.begin(), surfaces.end(), surface);

            if (p != surfaces.end())
            {
                surfaces.erase(p);
                surfaces.push_back(surface);

                ul.unlock();
                observers.surfaces_reordered();

                return;
            }
        }
    }

    BOOST_THROW_EXCEPTION(std::runtime_error("Invalid surface"));
}

void ms::SurfaceStack::raise(SurfaceSet const& ss)
{
    bool surfaces_reordered{false};

    {
        std::lock_guard<decltype(guard)> ul(guard);

        for (auto &layer : layers_by_depth)
        {
            auto &surfaces = layer.second;

            auto const old_surfaces = surfaces;

            std::stable_partition(
                begin(surfaces), end(surfaces),
                [&](std::weak_ptr<Surface> const& s) { return !ss.count(s); });

            if (old_surfaces != surfaces)
                surfaces_reordered = true;
        }
    }

    if (surfaces_reordered)
        observers.surfaces_reordered();
}

void ms::SurfaceStack::create_rendering_tracker_for(std::shared_ptr<Surface> const& surface)
{
    auto const tracker = std::make_shared<RenderingTracker>(surface);
    tracker->active_compositors(registered_compositors);
    rendering_trackers[surface.get()] = tracker;
}

void ms::SurfaceStack::update_rendering_tracker_compositors()
{
    for (auto const& pair : rendering_trackers)
        pair.second->active_compositors(registered_compositors);
}

void ms::SurfaceStack::add_observer(std::shared_ptr<ms::Observer> const& observer)
{
    observers.add(observer);

    // Notify observer of existing surfaces
    {
        std::lock_guard<decltype(guard)> ul(guard);
        for (auto &layer : layers_by_depth)
        {
            for (auto &surface : layer.second)
                observer->surface_exists(surface.get());
        }
    }
}

void ms::SurfaceStack::remove_observer(std::weak_ptr<ms::Observer> const& observer)
{
    auto o = observer.lock();
    if (!o)
        BOOST_THROW_EXCEPTION(std::logic_error("Invalid observer (destroyed)"));
    
    o->end_observation();
    
    observers.remove(o);
}

void ms::Observers::surface_added(ms::Surface* surface) 
{
    for_each([&](std::shared_ptr<Observer> const& observer)
        { observer->surface_added(surface); });
}

void ms::Observers::surface_removed(ms::Surface* surface)
{
    for_each([&](std::shared_ptr<Observer> const& observer)
        { observer->surface_removed(surface); });
}

void ms::Observers::surfaces_reordered()
{
    for_each([&](std::shared_ptr<Observer> const& observer)
        { observer->surfaces_reordered(); });
}

void ms::Observers::scene_changed()
{
   for_each([&](std::shared_ptr<Observer> const& observer)
        { observer->scene_changed(); });
}

void ms::Observers::surface_exists(ms::Surface* surface)
{
    for_each([&](std::shared_ptr<Observer> const& observer)
        { observer->surface_exists(surface); });
}

void ms::Observers::end_observation()
{
    for_each([&](std::shared_ptr<Observer> const& observer)
        { observer->end_observation(); });
}
