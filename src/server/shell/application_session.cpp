/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <racarr@canonical.com>
 */

#include "mir/shell/application_session.h"
#include "surface.h"

#include "mir/shell/surface_factory.h"

#include <boost/throw_exception.hpp>

#include <stdexcept>
#include <memory>
#include <cassert>
#include <algorithm>

namespace mf = mir::frontend;
namespace msh = mir::shell;

msh::ApplicationSession::ApplicationSession(
    std::shared_ptr<SurfaceFactory> const& surface_factory,
    std::string const& session_name) :
    surface_factory(surface_factory),
    session_name(session_name),
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

mf::SurfaceId msh::ApplicationSession::create_surface(const mf::SurfaceCreationParameters& params)
{
    auto surf = surface_factory->create_surface(params);
    auto const id = next_id();

    std::unique_lock<std::mutex> lock(surfaces_mutex);
    surfaces[id] = surf;
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

void msh::ApplicationSession::destroy_surface(mf::SurfaceId id)
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    auto p = checked_find(id);

    p->second->destroy();
    surfaces.erase(p);
}

std::string msh::ApplicationSession::name() const
{
    return session_name;
}

std::string msh::ApplicationSession::urn() const
{
    // We could go through the pain of fully validating the URN but there's
    // no real value in that right now.

    return session_name.compare(0, 4, "urn:") == 0 ? session_name :
           std::string();
}

void msh::ApplicationSession::shutdown()
{
    std::unique_lock<std::mutex> lock(surfaces_mutex);
    for (auto& id_s : surfaces)
    {
        id_s.second->shutdown();
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
