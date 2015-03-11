/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/events/event_builders.h"
#include "mir/scene/surface_event_source.h"
#include "mir/scene/surface.h"

#include <cstring>
#include <algorithm>

namespace ms = mir::scene;
namespace mev = mir::events;
namespace geom = mir::geometry;

ms::SurfaceEventSource::SurfaceEventSource(
    frontend::SurfaceId id,
    std::shared_ptr<frontend::EventSink> const& event_sink) :
    id(id),
    event_sink(event_sink)
{
}

void ms::SurfaceEventSource::surface_changed(ms::Surface const& surface,
                                             SurfaceObserver::Change change)
{
    switch (change)
    {
    case size:
        event_sink->handle_event(*mev::make_event(id, surface.size()));
        break;
    case close:
        event_sink->handle_event(*mev::make_event(id));
        break;
    case type:
        attrib_changed(surface, mir_surface_attrib_type);
        break;
    case state:
        attrib_changed(surface, mir_surface_attrib_state);
        break;
    case swapinterval:
        attrib_changed(surface, mir_surface_attrib_swapinterval);
        break;
    case focus:
        attrib_changed(surface, mir_surface_attrib_focus);
        break;
    case dpi:
        attrib_changed(surface, mir_surface_attrib_dpi);
        break;
    case pref_orientation:
        attrib_changed(surface, mir_surface_attrib_preferred_orientation);
        break;
    case orientation:
        event_sink->handle_event(*mev::make_event(id, surface.orientation()));
        break;
    default:
        break;
    }
}

void ms::SurfaceEventSource::attrib_changed(Surface const& s,
                                            MirSurfaceAttrib a) const
{
    event_sink->handle_event(*mev::make_event(id, a, s.query(a)));
}

// TODO
#if 0
void ms::SurfaceEventSource::keymap_changed(xkb_rule_names const& names)
{
    event_sink->handle_event(*mev::make_event(id, names));
}
#endif
