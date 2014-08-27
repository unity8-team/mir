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

// local
#include "application.h"
#include "application_manager.h"
#include "desktopfilereader.h"
#include "taskcontroller.h"

// common
#include <debughelpers.h>

// QPA mirserver
#include "logging.h"

// mir
#include <mir/scene/session.h>
#include <mir/scene/snapshot.h>
#include <mir/scene/prompt_session_manager.h>

namespace ms = mir::scene;

namespace qtmir
{
QMutex screenshotMutex;

Application::Application(const QSharedPointer<TaskController>& taskController,
                         DesktopFileReader *desktopFileReader,
                         State state,
                         const QStringList &arguments,
                         const std::shared_ptr<ms::PromptSessionManager>& promptSessionManager,
                         ApplicationManager *parent)
    : ApplicationInfoInterface(desktopFileReader->appId(), parent)
    , m_appMgr(parent)
    , m_taskController(taskController)
    , m_desktopData(desktopFileReader)
    , m_pid(0)
    , m_stage((m_desktopData->stageHint() == "SideStage") ? Application::SideStage : Application::MainStage)
    , m_state(state)
    , m_focused(false)
    , m_canBeResumed(true)
    , m_fullscreen(false)
    , m_arguments(arguments)
    , m_suspendTimer(new QTimer(this))
    , m_surface(nullptr)
    , m_promptSessionManager(promptSessionManager)
    , m_screenShotGuard(new Guard)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::Application - appId=" << desktopFileReader->appId() << "state=" << state;

    m_suspendTimer->setSingleShot(true);
    connect(m_suspendTimer, SIGNAL(timeout()), this, SLOT(suspend()));

    // FIXME(greyback) need to save long appId internally until ubuntu-app-launch can hide it from us
    m_longAppId = desktopFileReader->file().remove(QRegExp(".desktop$")).split('/').last();

    // FIXME: This is a hack. Remove once we have a real implementation for knowing the supported
    // orientations of an app
    m_supportedOrientations = PortraitOrientation
        | LandscapeOrientation
        | InvertedPortraitOrientation
        | InvertedLandscapeOrientation;
}

Application::~Application()
{
    QList<MirSurfaceItem*> promptSurfaces(m_promptSurfaces);
    for (MirSurfaceItem* promptSurface : promptSurfaces) {
        delete promptSurface;
    }

    qCDebug(QTMIR_APPLICATIONS) << "Application::~Application";
    {
        // In case we get a threaded screenshot callback once the application is deleted.
        QMutexLocker lk(&screenshotMutex);
        m_screenShotGuard.clear();
    }
    stopPromptSessions();
    delete m_surface;
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

Application::Stages Application::supportedStages() const
{
    return m_supportedStages;
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

std::shared_ptr<mir::scene::Session> Application::session() const
{
    return m_session;
}

bool Application::canBeResumed() const
{
    return m_canBeResumed;
}

void Application::setCanBeResumed(const bool resume)
{
    m_canBeResumed = resume;
}

pid_t Application::pid() const
{
    return m_pid;
}

void Application::setPid(pid_t pid)
{
    m_pid = pid;
}

void Application::setSession(const std::shared_ptr<mir::scene::Session>& session)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setSession - appId=" << appId() << "session=" << session.get();

    m_session = session;
}

void Application::setStage(Application::Stage stage)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setStage - appId=" << appId() << "stage=" << stage;

    if (m_stage != stage) {
        if (stage | m_supportedStages) {
            return;
        }

        m_stage = stage;
        Q_EMIT stageChanged(stage);

        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleStage);
        return;
    }
}

void Application::setState(Application::State state)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setState - appId=" << appId() << "state=" << applicationStateToStr(state);
    if (m_state != state) {
        switch (state)
        {
        case Application::Suspended:
            if (m_state == Application::Running) {
                stopPromptSessions();
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
            stopPromptSessions();
            if (m_suspendTimer->isActive())
                m_suspendTimer->stop();
            if (m_surface) {
                m_surface->stopFrameDropper();
            }
            break;
        default:
            break;
        }
        m_state = state;
        Q_EMIT stateChanged(state);

        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleState);
    }
}

void Application::setFocused(bool focused)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setFocused - appId=" << appId() << "focused=" << focused;
    if (m_focused != focused) {
        m_focused = focused;
        Q_EMIT focusedChanged(focused);
        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleFocused);
    }
}

void Application::setFullscreen(bool fullscreen)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setFullscreen - appId=" << appId() << "fullscreen=" << fullscreen;
    if (m_fullscreen != fullscreen) {
        m_fullscreen = fullscreen;
        Q_EMIT fullscreenChanged();
        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleFullscreen);
    }
}

void Application::suspend()
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::suspend - appId=" << appId();
    if (m_surface) {
        m_surface->stopFrameDropper();
    } else {
        qDebug() << "Application::suspend - no surface to call stopFrameDropper() on!";
    }
    m_taskController->suspend(longAppId());
}

void Application::resume()
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::resume - appId=" << appId();
    if (m_surface) {
        m_surface->startFrameDropper();
    }
    m_taskController->resume(longAppId());
}

void Application::respawn()
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::respawn - appId=" << appId();
    m_taskController->start(appId(), m_arguments);
}

QString Application::longAppId() const
{
    return m_longAppId;
}

Application::SupportedOrientations Application::supportedOrientations() const
{
    return m_supportedOrientations;
}

MirSurfaceItem* Application::surface() const
{
    // Only notify QML of surface creation once it has drawn its first frame.
    if (m_surface && m_surface->isFirstFrameDrawn()) {
        return m_surface;
    } else {
        return nullptr;
    }
}

void Application::setSurface(MirSurfaceItem *newSurface)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setSurface - appId=" << appId() << "surface=" << newSurface;

    if (newSurface == m_surface) {
        return;
    }

    if (m_surface) {
        m_surface->disconnect(this);
        m_surface->setApplication(nullptr);
        m_surface->setParent(nullptr);
    }

    MirSurfaceItem *previousSurface = surface();
    m_surface = newSurface;

    if (newSurface) {
        m_surface->setParent(this);
        m_surface->setApplication(this);

        // Only notify QML of surface creation once it has drawn its first frame.
        if (!surface()) {
            connect(newSurface, &MirSurfaceItem::firstFrameDrawn,
                    this, &Application::emitSurfaceChanged);
        }

        connect(newSurface, &MirSurfaceItem::surfaceDestroyed,
                this, &Application::discardSurface);

        connect(newSurface, &MirSurfaceItem::stateChanged,
            this, &Application::updateFullscreenProperty);
    }

    if (previousSurface != surface()) {
        emitSurfaceChanged();
    }

    updateFullscreenProperty();
}

void Application::emitSurfaceChanged()
{
    Q_EMIT surfaceChanged();
    QModelIndex appIndex = m_appMgr->findIndex(this);
    Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleSurface);
}

void Application::discardSurface()
{
    MirSurfaceItem *discardedSurface = m_surface;
    setSurface(nullptr);
    delete discardedSurface;
}

void Application::updateFullscreenProperty()
{
    if (m_surface) {
        setFullscreen(m_surface->state() == MirSurfaceItem::Fullscreen);
    } else {
        // Keep the current value of the fullscreen property until we get a new
        // surface
    }
}

void Application::appendPromptSession(const std::shared_ptr<ms::PromptSession>& promptSession)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::appendPromptSession appId=" << appId()
        << "promptSession=" << (promptSession ? promptSession.get() : nullptr);

    m_promptSessions.append(promptSession);
}

void Application::removePromptSession(const std::shared_ptr<ms::PromptSession>& promptSession)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::removePromptSession appId=" << appId()
        << "promptSession=" << (promptSession ? promptSession.get() : nullptr);

    m_promptSessions.removeAll(promptSession);
}

void Application::stopPromptSessions()
{
    QList<std::shared_ptr<ms::PromptSession>> copy(m_promptSessions);

    QListIterator<std::shared_ptr<ms::PromptSession>> it(copy);
    for ( it.toBack(); it.hasPrevious(); ) {
        m_promptSessionManager->stop_prompt_session(it.previous());
    }
}

std::shared_ptr<ms::PromptSession> Application::activePromptSession() const
{
    if (m_promptSessions.count() > 0)
        return m_promptSessions.back();
    return nullptr;
}

void Application::foreachPromptSession(std::function<void(const std::shared_ptr<mir::scene::PromptSession>&)> f) const
{
    for (std::shared_ptr<mir::scene::PromptSession> promptSession : m_promptSessions) {
        f(promptSession);
    }
}

bool Application::containsProcess(pid_t pid) const
{
    if (m_pid == pid)
        return true;

    QListIterator<std::shared_ptr<ms::PromptSession>> iter(m_promptSessions);
    while(iter.hasNext()) {
        std::shared_ptr<ms::PromptSession> promptSession = iter.next();

        std::shared_ptr<ms::Session> helper = m_promptSessionManager->helper_for(promptSession);
        if (helper && helper->process_id() == pid)
            return true;

        bool found = false;
        m_promptSessionManager->for_each_provider_in(promptSession,
            [&found, pid](std::shared_ptr<ms::Session> const& provider) {
                if (provider->process_id() == pid)
                    found = true;
            });
        if (found)
            return true;
    }
    return false;
}

void Application::addPromptSurface(MirSurfaceItem* surface)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::addPromptSurface " << surface->name() << " to " << name();
    if (surface == m_surface || m_promptSurfaces.contains(surface))
        return;

    surface->setApplication(this);
    m_promptSurfaces.append(surface);
    Q_EMIT promptSurfacesChanged();
}

void Application::removeSurface(MirSurfaceItem* surface)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::removeSurface " << surface->name() << " from " << name();

    if (m_surface == surface) {
        setSurface(nullptr);
    } else if (m_promptSurfaces.contains(surface)) {
        m_promptSurfaces.removeAll(surface);
        surface->setApplication(nullptr);

        Q_EMIT promptSurfacesChanged();
    }
}

void Application::foreachPromptSurface(std::function<void(MirSurfaceItem*)> f) const
{
    for (MirSurfaceItem* promptSurface : m_promptSurfaces) {
        f(promptSurface);
    }
}

QQmlListProperty<MirSurfaceItem> Application::promptSurfaces()
{
    return QQmlListProperty<MirSurfaceItem>(this,
                                            0,
                                            Application::promptSurfaceCount,
                                            Application::promptSurfaceAt);
}

int Application::promptSurfaceCount(QQmlListProperty<MirSurfaceItem> *prop)
{
    Application *p = qobject_cast<Application*>(prop->object);
    return p->m_promptSurfaces.count();
}

MirSurfaceItem* Application::promptSurfaceAt(QQmlListProperty<MirSurfaceItem> *prop, int index)
{
    Application *p = qobject_cast<Application*>(prop->object);

    if (index < 0 || index >= p->m_promptSurfaces.count())
        return nullptr;
    return p->m_promptSurfaces[index];
}

} // namespace qtmir
