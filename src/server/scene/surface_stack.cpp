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
        std::shared_ptr<mg::Renderable> renderable,
        std::shared_ptr<ms::RenderingTracker> const& tracker)
        : renderable_{renderable},
          tracker{tracker}
    {
    }

    std::shared_ptr<mg::Renderable> renderable() const override
    {
        return renderable_;
    }

    void rendered_in(mc::CompositorID cid) override
    {
        tracker->rendered_in(cid);
    }

    void occluded_in(mc::CompositorID cid) override
    {
        tracker->occluded_in(cid);
    }

private:
    std::shared_ptr<mg::Renderable> const renderable_;
    std::shared_ptr<ms::RenderingTracker> const tracker;
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
    mc::SceneElementSequence elements;
    for (auto const& layer : layers_by_depth)
    {
        for (auto const& surface : layer.second) 
        {
            auto element = std::make_shared<SurfaceSceneElement>(
                surface->compositor_snapshot(id),
                rendering_trackers[surface.get()]);
            elements.emplace_back(element);
        }
    }
    return elements;
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

void ms::SurfaceStack::for_each(std::function<void(std::shared_ptr<mi::Surface> const&)> const& callback)
{
    std::lock_guard<decltype(guard)> lg(guard);
    for (auto &layer : layers_by_depth)
    {
        for (auto it = layer.second.begin(); it != layer.second.end(); ++it)
            callback(*it);
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
    observers.add_observer(observer);

    // Notify observer of existing surfaces
    {
        std::unique_lock<decltype(guard)> ul(guard);
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
    
    observers.remove_observer(o);
}

void ms::Observers::surface_added(ms::Surface* surface) 
{
    std::unique_lock<decltype(mutex)> lg(mutex);
    
    for (auto observer : observers)
        observer->surface_added(surface);
}

void ms::Observers::surface_removed(ms::Surface* surface)
{
    std::unique_lock<decltype(mutex)> lg(mutex);

    for (auto observer : observers)
        observer->surface_removed(surface);
}

void ms::Observers::surfaces_reordered()
{
    std::unique_lock<decltype(mutex)> lg(mutex);
    
    for (auto observer : observers)
        observer->surfaces_reordered();
}

void ms::Observers::surface_exists(ms::Surface* surface)
{
    std::unique_lock<decltype(mutex)> lg(mutex);
    
    for (auto observer : observers)
        observer->surface_exists(surface);
}

void ms::Observers::end_observation()
{
    std::unique_lock<decltype(mutex)> lg(mutex);
    
    for (auto observer : observers)
        observer->end_observation();
}

void ms::Observers::add_observer(std::shared_ptr<ms::Observer> const& observer)
{
    std::unique_lock<decltype(mutex)> lg(mutex);

    observers.push_back(observer);
}

void ms::Observers::remove_observer(std::shared_ptr<ms::Observer> const& observer)
{
    std::unique_lock<decltype(mutex)> lg(mutex);
    
    auto it = std::find(observers.begin(), observers.end(), observer);
    if (it == observers.end())
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid observer (not previously added)"));
    
    observers.erase(it);
}
