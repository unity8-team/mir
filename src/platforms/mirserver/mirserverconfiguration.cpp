/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "mirserverconfiguration.h"

#include "connectioncreator.h"
#include "mirglconfig.h"
#include "mirplacementstrategy.h"
#include "mirinputdispatcherconfiguration.h"
#include "mirserverstatuslistener.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "sessionauthorizer.h"
#include "qtcompositor.h"
#include "logging.h"
#include "unityprotobufservice.h"

#include <QDebug>

namespace msh = mir::shell;
namespace ms = mir::scene;

MirServerConfiguration::MirServerConfiguration(int argc, char const* argv[], QObject* parent)
    : QObject(parent)
    , DefaultServerConfiguration(argc, argv)
    , m_unityService(std::make_shared<UnityProtobufService>())
{
    DLOG("MirServerConfiguration created");
}

std::shared_ptr<ms::PlacementStrategy>
MirServerConfiguration::the_placement_strategy()
{
    return shell_placement_strategy(
        [this]
        {
            return std::make_shared<MirPlacementStrategy>(the_shell_display_layout());
        });
}

std::shared_ptr<ms::SessionListener>
MirServerConfiguration::the_session_listener()
{
    return session_listener(
        [this]
        {
            return std::make_shared<SessionListener>();
        });
}

std::shared_ptr<ms::SurfaceConfigurator>
MirServerConfiguration::the_surface_configurator()
{
    return surface_configurator(
        [this]()
        {
            return std::make_shared<SurfaceConfigurator>();
        });
}

std::shared_ptr<mir::frontend::SessionAuthorizer>
MirServerConfiguration::the_session_authorizer()
{
    return session_authorizer(
    []
    {
        return std::make_shared<SessionAuthorizer>();
    });
}

std::shared_ptr<mir::compositor::Compositor>
MirServerConfiguration::the_compositor()
{
    return compositor(
        [this]()
        {
            return std::make_shared<QtCompositor>();
        });
}

std::shared_ptr<mir::input::InputDispatcherConfiguration>
MirServerConfiguration::the_input_dispatcher_configuration()
{
    return input_dispatcher_configuration(
    [this]()
    {
        return std::make_shared<MirInputDispatcherConfiguration>();
    });
}

std::shared_ptr<mir::graphics::GLConfig>
MirServerConfiguration::the_gl_config()
{
    return gl_config(
    [this]()
    {
        return std::make_shared<MirGLConfig>();
    });
}

std::shared_ptr<mir::ServerStatusListener>
MirServerConfiguration::the_server_status_listener()
{
    return server_status_listener(
        []()
        {
            return std::make_shared<MirServerStatusListener>();
        });
}

std::shared_ptr<mir::frontend::ConnectionCreator>
MirServerConfiguration::the_connection_creator()
{
    return connection_creator([this]
        {
            return std::make_shared<ConnectionCreator>(
                m_unityService,
                the_ipc_factory(the_frontend_shell(), the_buffer_allocator()),
                the_session_authorizer(),
                the_message_processor_report());
        });
}

/************************************ Shell side ************************************/

//
// Note about the
//     if (sharedPtr.unique()) return 0;
// constructs used in the functions below.
// The rationale is that if when you do
//     the_session_authorizer()
// get a pointer that is unique means that Mir is not
// holding the pointer and thus when we return from the
//     sessionAuthorizer()
// scope the unique pointer will be destroyed so we return 0
//

SessionAuthorizer *MirServerConfiguration::sessionAuthorizer()
{
    auto sharedPtr = the_session_authorizer();
    if (sharedPtr.unique()) return 0;

    return static_cast<SessionAuthorizer*>(sharedPtr.get());
}

SessionListener *MirServerConfiguration::sessionListener()
{
    auto sharedPtr = the_session_listener();
    if (sharedPtr.unique()) return 0;

    return static_cast<SessionListener*>(sharedPtr.get());
}

SurfaceConfigurator *MirServerConfiguration::surfaceConfigurator()
{
    auto sharedPtr = the_surface_configurator();
    if (sharedPtr.unique()) return 0;

    return static_cast<SurfaceConfigurator*>(sharedPtr.get());
}
