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

#ifndef SHELLSERVERCONFIGURATION_H
#define SHELLSERVERCONFIGURATION_H

#include <QObject>
#include <mir/default_server_configuration.h>


class SessionListener;
class SessionAuthorizer;
class SurfaceConfigurator;

class ShellServerConfiguration : public QObject, public mir::DefaultServerConfiguration
{
    Q_OBJECT

    Q_PROPERTY(SessionAuthorizer* sessionAuthorizer READ sessionAuthorizer CONSTANT)
    Q_PROPERTY(SessionListener* sessionListener READ sessionListener CONSTANT)
    Q_PROPERTY(SurfaceConfigurator* surfaceConfigurator READ surfaceConfigurator CONSTANT)

public:
    ShellServerConfiguration(int argc, char const* argv[], QObject* parent = 0);
    ~ShellServerConfiguration();

    /* mir specific */
    std::shared_ptr<mir::shell::PlacementStrategy> the_shell_placement_strategy() override;
    std::shared_ptr<mir::shell::SessionListener> the_shell_session_listener() override;
    std::shared_ptr<mir::shell::SurfaceConfigurator> the_shell_surface_configurator() override;
    std::shared_ptr<mir::frontend::SessionAuthorizer> the_session_authorizer() override;

    /* qt specific */
    // getters
    SessionAuthorizer *sessionAuthorizer();
    SessionListener *sessionListener();
    SurfaceConfigurator *surfaceConfigurator();

private:
};

#endif // SHELLSERVERCONFIGURATION_H
