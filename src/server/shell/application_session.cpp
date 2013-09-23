/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Robert Carr <racarr@canonical.com>
 */

#include "mir/shell/application_session.h"
#include "mir/shell/surface.h"
#include "mir/shell/surface_factory.h"
#include "mir/shell/snapshot_strategy.h"
#include "mir/shell/session_listener.h"
#include "mir/shell/input_targeter.h"
#include "mir/frontend/event_sink.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <memory>
#include <cassert>
#include <algorithm>

namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace mg = mir::graphics;

msh::ApplicationSession::ApplicationSession(
    std::shared_ptr<SurfaceFactory> const& surface_factory,
    std::string const& session_name,
    std::shared_ptr<SnapshotStrategy> const& snapshot_strategy,
    std::shared_ptr<msh::SessionListener> const& session_listener,
    std::shared_ptr<mf::EventSink> const& sink) :
    surface_factory(surface_factory),
    session_name(session_name),
    snapshot_strategy(snapshot_strategy),
    session_listener(session_listener),
    event_sink(sink),
    next_surface_id(0)
{
    assert(surface_factory);
}

msh::ApplicationSession::~ApplicationSession()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto const& pair_id_surface : surfaces)
    {
        pair_id_surface.second->destroy();
    }
}

mf::SurfaceId msh::ApplicationSession::next_id()
{
    return mf::SurfaceId(next_surface_id.fetch_add(1));
}

mf::SurfaceId msh::ApplicationSession::create_surface(const msh::SurfaceCreationParameters& params)
{
    auto const id = next_id();
    auto surf = surface_factory->create_surface(this, params, id, event_sink);

    std::unique_lock<std::mutex> lock(surfaces_mutex);
    surfaces[id] = surf;
    
    session_listener->surface_created(*this, surf);
    
    return id;
}

msh::ApplicationSession::Surfaces::const_iterator msh::ApplicationSession::checked_find(mf::SurfaceId id) const
{
    auto p = surfaces.find(id);
    if (p == surfaces.end())
    {
        BOOST_THROW_EXCEPTION(std::runtime_error("Invalid SurfaceId"));
    }
    return p;
}

std::shared_ptr<mf::Surface> msh::ApplicationSession::get_surface(mf::SurfaceId id) const
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);

    return checked_find(id)->second;
}

void msh::ApplicationSession::take_snapshot(SnapshotCallback const& snapshot_taken)
{
    snapshot_strategy->take_snapshot_of(default_surface(), snapshot_taken);
}

std::shared_ptr<msh::Surface> msh::ApplicationSession::default_surface_locked(std::unique_lock<std::mutex> const& /* lock */) const
{
    if (surfaces.size())
        return surfaces.begin()->second;
    else
        return std::shared_ptr<msh::Surface>();
}

std::shared_ptr<msh::Surface> msh::ApplicationSession::default_surface() const
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);

    return default_surface_locked(lock);
}

void msh::ApplicationSession::destroy_surface(mf::SurfaceId id)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    auto p = checked_find(id);
    
    session_listener->destroying_surface(*this, p->second);

    p->second->destroy();
    surfaces.erase(p);
}

std::string msh::ApplicationSession::name() const
{
    return session_name;
}

void msh::ApplicationSession::force_requests_to_complete()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto& id_s : surfaces)
    {
        id_s.second->force_requests_to_complete();
    }
}

void msh::ApplicationSession::hide()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto& id_s : surfaces)
    {
        id_s.second->hide();
    }
}

void msh::ApplicationSession::show()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto& id_s : surfaces)
    {
        id_s.second->show();
    }
}

int msh::ApplicationSession::configure_surface(mf::SurfaceId id,
                                               MirSurfaceAttrib attrib,
                                               int requested_value)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    std::shared_ptr<msh::Surface> surf(checked_find(id)->second);

    return surf->configure(attrib, requested_value);
}

void msh::ApplicationSession::send_display_config(mg::DisplayConfiguration const& info)
{
    event_sink->handle_display_config_change(info);
}

void msh::ApplicationSession::set_lifecycle_state(MirLifecycleState state)
{
    event_sink->handle_lifecycle_event(state);
}

void msh::ApplicationSession::receive_focus(std::shared_ptr<msh::InputTargeter> const& targeter,
                                            std::shared_ptr<SurfaceController> const& controller)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    if (surfaces.size() == 0)
    {
        targeter->focus_cleared();
        return;
    }
    // Use default_surface_locked idiom instead
    auto focus_surface = surfaces.begin()->second;
    
    focus_surface->raise(controller);
    focus_surface->take_input_focus(targeter);
    focus_surface->configure(mir_surface_attrib_focus, mir_surface_focused);

    last_focused_surface = focus_surface;
}

void msh::ApplicationSession::relinquish_focus()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    auto surf = default_surface_locked(lock);
    if (surf)
    {
        surf->configure(mir_surface_attrib_focus, mir_surface_unfocused);
    }
}
