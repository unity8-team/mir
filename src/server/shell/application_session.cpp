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
#include "mir/surfaces/surface.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <memory>
#include <cassert>
#include <algorithm>

namespace me = mir::events;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace ms = mir::surfaces;

msh::ApplicationSession::ApplicationSession(
    std::string const& session_name,
    std::shared_ptr<me::EventSink> const& sink)
    : session_name(session_name),
      event_sink(sink),
      next_surface_id(0)
{
}

msh::ApplicationSession::ApplicationSession(
    std::string const& session_name)
    : ApplicationSession(session_name, std::shared_ptr<me::EventSink>())
{
}

msh::ApplicationSession::~ApplicationSession()
{
}

mf::SurfaceId msh::ApplicationSession::associate_surface(std::weak_ptr<ms::Surface> const& surface,
                                                         std::shared_ptr<mf::Surface> const& shell_surface)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    mf::SurfaceId id{next_surface_id++};

    auto association = std::make_pair(shell_surface, surface);
    surfaces[id] = association; 
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

    return (checked_find(id)->second).first;
}

std::shared_ptr<mf::Surface> msh::ApplicationSession::default_surface() const
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);

    if (surfaces.size())
        return (surfaces.begin()->second).first;
    else
        return std::shared_ptr<msh::Surface>();
}

void msh::ApplicationSession::disassociate_surface(mf::SurfaceId id)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    auto p = checked_find(id);

    (p->second).first->destroy();
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
        (id_s.second).first->force_requests_to_complete();
    }
}

void msh::ApplicationSession::hide()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto& id_s : surfaces)
    {
        (id_s.second).first->hide();
    }
}

void msh::ApplicationSession::show()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto& id_s : surfaces)
    {
        (id_s.second).first->show();
    }
}

int msh::ApplicationSession::configure_surface(mf::SurfaceId id,
                                               MirSurfaceAttrib attrib,
                                               int value)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    std::shared_ptr<mf::Surface> surf((checked_find(id)->second).first);

    return surf->configure(attrib, value);
}
