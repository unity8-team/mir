/*
 * Copyright (C) 2013 Canonical, Ltd.
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

#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "sessionauthorizer.h"
#include "voidcompositor.h"
#include "logging.h"

#include <QDebug>

namespace msh = mir::shell;
namespace ms = mir::surfaces;

MirServerConfiguration::MirServerConfiguration(int argc, char const* argv[], QObject* parent)
    : QObject(parent)
    , DefaultServerConfiguration(argc, argv)
{
    DLOG("MirServerConfiguration created");
}

MirServerConfiguration::~MirServerConfiguration()
{
    qDebug() << "~MirServerConfiguration";
}

std::shared_ptr<msh::SessionListener>
MirServerConfiguration::the_shell_session_listener()
{
    return shell_session_listener(
        [this]
        {
            return std::make_shared<SessionListener>();
        });
}

std::shared_ptr<msh::SurfaceConfigurator>
MirServerConfiguration::the_shell_surface_configurator()
{
    return shell_surface_configurator(
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
            return std::make_shared<VoidCompositor>();
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
    auto sharedPtr = the_shell_session_listener();
    if (sharedPtr.unique()) return 0;

    return static_cast<SessionListener*>(sharedPtr.get());
}

SurfaceConfigurator *MirServerConfiguration::surfaceConfigurator()
{
    auto sharedPtr = the_shell_surface_configurator();
    if (sharedPtr.unique()) return 0;

    return static_cast<SurfaceConfigurator*>(sharedPtr.get());
}
