/*
 * Copyright (C) 2013 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "application_instance_mirserver_priv.h"

#include "mircommon/application_description_mir_priv.h"
#include "mircommon/application_options_mir_priv.h"
#include "mircommon/application_id_mir_priv.h"

#include <mir/scene/surface.h>
#include <mir/scene/surface_coordinator.h>
#include <mir/shell/surface_creation_parameters.h>

namespace uam = ubuntu::application::mir;
namespace uams = uam::server;

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;

uams::Instance::Instance(std::shared_ptr<ms::SurfaceCoordinator> const &surface_coordinator,
                         uam::Description* description_,
                         uam::Options *options_)
    : surface_coordinator(surface_coordinator),
      ref_count(1)
{
    description = DescriptionPtr(description_, 
        [] (uam::Description* p)
        {
            delete p;
        });
    options = OptionsPtr(options_,
        [] (uam::Options* p)
        {
            delete p;
        });
}

UApplicationInstance* uams::Instance::as_u_application_instance()
{
    return static_cast<UApplicationInstance*>(this);
}

uams::Instance* uams::Instance::from_u_application_instance(UApplicationInstance *u_instance)
{
    return static_cast<uams::Instance*>(u_instance);
}

void uams::Instance::ref()
{
    ref_count++;
}

void uams::Instance::unref()
{
    ref_count--;
    if (ref_count == 0)
        delete this;
}

std::shared_ptr<ms::Surface> uams::Instance::create_surface(msh::SurfaceCreationParameters const& parameters)
{
    return surface_coordinator->add_surface(parameters, nullptr, {});
}
