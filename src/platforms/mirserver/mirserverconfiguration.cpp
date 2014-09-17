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

// local
#include "focussetter.h"
#include "mirglconfig.h"
#include "mirplacementstrategy.h"
#include "mirserverstatuslistener.h"
#include "promptsessionlistener.h"
#include "sessionlistener.h"
#include "surfaceconfigurator.h"
#include "sessionauthorizer.h"
#include "qtcompositor.h"
#include "qteventfeeder.h"
#include "logging.h"

// mir
#include <mir/options/default_configuration.h>

// Qt
#include <QDebug>

// egl
#include <EGL/egl.h>

namespace mo  = mir::options;
namespace msh = mir::shell;
namespace ms = mir::scene;

namespace
{
void ignore_unparsed_arguments(int /*argc*/, char const* const/*argv*/[])
{
}
}

Q_LOGGING_CATEGORY(QTMIR_MIR_MESSAGES, "qtmir.mir")

MirServerConfiguration::MirServerConfiguration(int argc, char const* argv[], QObject* parent)
    : QObject(parent)
    , DefaultServerConfiguration(std::make_shared<mo::DefaultConfiguration>(argc, argv, &ignore_unparsed_arguments))
{
    qCDebug(QTMIR_MIR_MESSAGES) << "MirServerConfiguration created";
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

std::shared_ptr<ms::PromptSessionListener>
MirServerConfiguration::the_prompt_session_listener()
{
    return prompt_session_listener(
        [this]
        {
            return std::make_shared<PromptSessionListener>();
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

std::shared_ptr<mir::input::InputDispatcher>
MirServerConfiguration::the_input_dispatcher()
{
    return input_dispatcher(
    [this]()
    {
        return std::make_shared<QtEventFeeder>();
    });
}

std::shared_ptr<mir::graphics::GLConfig>
MirServerConfiguration::the_gl_config()
{
    return gl_config(
    [this]()
    {
#ifdef QTMIR_USE_OPENGL
        // Should desktop-GL be desired, need to bind that API before a context is created
        eglBindAPI(EGL_OPENGL_API);
#endif
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

std::shared_ptr<mir::shell::FocusSetter>
MirServerConfiguration::the_shell_focus_setter()
{
    return shell_focus_setter(
        [this]
        {
            return std::make_shared<FocusSetter>();
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

PromptSessionListener *MirServerConfiguration::promptSessionListener()
{
    auto sharedPtr = the_prompt_session_listener();
    if (sharedPtr.unique()) return 0;

    return static_cast<PromptSessionListener*>(sharedPtr.get());
}

SurfaceConfigurator *MirServerConfiguration::surfaceConfigurator()
{
    auto sharedPtr = the_surface_configurator();
    if (sharedPtr.unique()) return 0;

    return static_cast<SurfaceConfigurator*>(sharedPtr.get());
}
