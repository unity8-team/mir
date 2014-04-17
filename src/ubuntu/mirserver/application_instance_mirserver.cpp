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
#include <mir/shell/placement_strategy.h>
#include <mir/shell/session.h>
#include <mir/shell/session_listener.h>
#include <mir/shell/surface_creation_parameters.h>

namespace uam = ubuntu::application::mir;
namespace uams = uam::server;

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace msh = mir::shell;

namespace
{
/* A Mir in-process client does not have an associated Session by default. However it is
 * useful for the shell to be able to position and identify its own surface, so need to
 * create a mock implementation of Session for that respective Surface.
 */
class InProcessClientSession : public msh::Session
{
public:
    virtual void force_requests_to_complete() override {}
    virtual pid_t process_id() const override { return 0; }
    virtual void take_snapshot(msh::SnapshotCallback const&) override {}
    virtual std::shared_ptr<msh::Surface> default_surface() const override { return surface; }
    virtual void set_lifecycle_state(MirLifecycleState) override {}

    virtual mf::SurfaceId create_surface(msh::SurfaceCreationParameters const& ) override {}
    virtual void destroy_surface(mf::SurfaceId) override {}
    virtual std::shared_ptr<mf::Surface> get_surface(mf::SurfaceId) const override { return surface; }
    virtual std::string name() const override { return "Shell"; }
    virtual void hide() override {}
    virtual void show() override {}
    virtual void send_display_config(mir::graphics::DisplayConfiguration const&) override {}
    virtual int configure_surface(mf::SurfaceId, MirSurfaceAttrib, int) override { return 0; }

    virtual void begin_trust_session() {}
    virtual void end_trust_session() {}
private:
    std::shared_ptr<msh::Surface> const surface;
};

InProcessClientSession& global_session()
{
    static InProcessClientSession session;
    return session;
}
}

uams::Instance::Instance(std::shared_ptr<ms::SurfaceCoordinator> const &surface_coordinator,
                         std::shared_ptr<msh::PlacementStrategy> const &placement_strategy,
                         std::shared_ptr<msh::SessionListener> const &session_listener,
                         uam::Description* description_,
                         uam::Options *options_)
    : surface_coordinator(surface_coordinator),
      placement_strategy(placement_strategy),
      session_listener(session_listener),
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
    auto placed_params = placement_strategy->place(global_session(), parameters);

    auto surface = surface_coordinator->add_surface(placed_params, nullptr);

    // Need to call the SessionListener ourselves, else shell not notified of this surface creation
    session_listener->surface_created(global_session(), surface);
    return surface;
}
