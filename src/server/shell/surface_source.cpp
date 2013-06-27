/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/shell/surface_source.h"
#include "mir/shell/surface_builder.h"
#include "mir/shell/surface.h"
#include "mir/frontend/surface.h"
#include "mir/input/input_channel_factory.h"

#include <cassert>

namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mi = mir::input;
namespace mf = mir::frontend;


msh::SurfaceSource::SurfaceSource(std::shared_ptr<SurfaceBuilder> const& surface_builder)
    : surface_builder(surface_builder)
{
    assert(surface_builder);
}

std::shared_ptr<msh::Surface> msh::SurfaceSource::create_surface(
    shell::SurfaceCreationParameters const& params,
    frontend::SurfaceId id,
    std::shared_ptr<events::EventSink> const& sink)
{
    //TODO: this is still messy. We should only have a simple make_shared here, and the builder
    //should exist up in the SessionManager 
    auto surface = surface_builder->create_surface(params);
    auto fn = [this](std::weak_ptr<ms::Surface> s){
        surface_builder->destroy_surface(s);
    };
    return std::make_shared<Surface>(
        surface,
        fn,
        id,
        sink);
}

