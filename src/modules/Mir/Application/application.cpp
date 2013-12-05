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

// local
#include "application.h"
#include "application_manager.h"
#include "desktopfilereader.h"
#include "taskcontroller.h"

// unity-mir
#include "logging.h"

// mir
#include <mir/shell/session.h>

Application::Application(const QString &appId, Application::State state,
                         const QStringList &arguments, QObject *parent)
    : Application(new DesktopFileReader(appId), state, arguments, parent)
{
}

Application::Application(DesktopFileReader *desktopFileReader, State state,
                         const QStringList &arguments, QObject *parent)
    : ApplicationInfoInterface(desktopFileReader->appId(), parent)
    , m_desktopData(desktopFileReader)
    , m_pid(0)
    , m_stage((m_desktopData->stageHint() == "SideStage") ? Application::SideStage : Application::MainStage)
    , m_state(state)
    , m_focused(false)
    , m_fullscreen(false)
    , m_arguments(arguments)
    , m_suspendTimer(new QTimer(this))
{
    DLOG("Application::Application (this=%p, appId=%s, state=%d", this, qPrintable(desktopFileReader->appId()),
         static_cast<int>(state));

    m_suspendTimer->setSingleShot(true);
    connect(m_suspendTimer, SIGNAL(timeout()), this, SLOT(suspend()));
}

Application::~Application()
{
    DLOG("Application::~Application");
    delete m_desktopData;
}

bool Application::isValid() const
{
    return m_desktopData->loaded();
}

QString Application::desktopFile() const
{
    return m_desktopData->file();
}

QString Application::appId() const
{
    return m_desktopData->appId();
}

QString Application::name() const
{
    return m_desktopData->name();
}

QString Application::comment() const
{
    return m_desktopData->comment();
}

QUrl Application::icon() const
{
    QString iconString = m_desktopData->icon();
    QString pathString = m_desktopData->path();

    if (QFileInfo(iconString).exists()) {
        return QUrl(iconString);
    } else if (QFileInfo(pathString + '/' + iconString).exists()) {
        return QUrl(pathString + '/' + iconString);
    } else {
        return QUrl("image://theme/" + iconString);
    }
}

QString Application::exec() const
{
    return m_desktopData->exec();
}

Application::Stage Application::stage() const
{
    return m_stage;
}

Application::State Application::state() const
{
    return m_state;
}

bool Application::focused() const
{
    return m_focused;
}

bool Application::fullscreen() const
{
    return m_fullscreen;
}

std::shared_ptr<mir::shell::Session> Application::session() const
{
    return m_session;
}

pid_t Application::pid() const
{
    return m_pid;
}

void Application::setPid(pid_t pid)
{
    m_pid = pid;
}

void Application::setSession(const std::shared_ptr<mir::shell::Session>& session)
{
    DLOG("Application::setSession (this=%p, session=%p)", this, session.get());

    // TODO(greyback) what if called with new surface?
    m_session = session;
}

void Application::setSessionName(const QString& name)
{
    DLOG("Application::setSessionName (this=%p, name=%s)", this, name.toLatin1().constData());
    if (m_session) {
        LOG("Application::setSessionName should not be called once session exists");
        return;
    }
    m_sessionName = name;
}

void Application::setStage(Application::Stage stage)
{
    DLOG("Application::setStage (this=%p, stage=%d)", this, static_cast<int>(stage));
    if (m_stage != stage) {
        m_stage = stage;
        Q_EMIT stageChanged(stage);
    }
}

void Application::setState(Application::State state)
{
    DLOG("Application::setState (this=%p, state=%d)", this, static_cast<int>(state));
    if (m_state != state) {
        switch (state)
        {
        case Application::Suspended:
            if (m_state == Application::Running) {
                session()->set_lifecycle_state(mir_lifecycle_state_will_suspend);
                m_suspendTimer->start(3000);
            }
            break;
        case Application::Running:
            if (m_suspendTimer->isActive())
                m_suspendTimer->stop();

            if (m_state == Application::Suspended) {
                resume();
                session()->set_lifecycle_state(mir_lifecycle_state_resumed);
            } else if (m_state == Application::Stopped) {
                respawn();
                state = Application::Starting;
            }
            break;
        case Application::Stopped:
            if (m_suspendTimer->isActive())
                m_suspendTimer->stop();
            break;
        default:
            break;
        }
        m_state = state;
        Q_EMIT stateChanged(state);
    }
}

void Application::setFocused(bool focused)
{
    DLOG("Application::setFocused (this=%p, focus=%s)", this, focused ? "yes" : "no");
    if (m_focused != focused) {
        m_focused = focused;
        Q_EMIT focusedChanged(focused);
    }
}

void Application::setFullscreen(bool fullscreen)
{
    DLOG("Application::setFullscreen (this=%p, fullscreen=%s)", this, fullscreen ? "yes" : "no");
    if (m_fullscreen != fullscreen) {
        m_fullscreen = fullscreen;
        Q_EMIT fullscreenChanged();
    }
}

void Application::suspend()
{
    DLOG("Application::suspend (this=%p)", this);
    TaskController::singleton()->suspend(appId());
}

void Application::resume()
{
    DLOG("Application::resume (this=%p)", this);
    TaskController::singleton()->resume(appId());
}

void Application::respawn()
{
    DLOG("Application::respawn (this=%p)", this);
    TaskController::singleton()->start(appId(), m_arguments);
}
