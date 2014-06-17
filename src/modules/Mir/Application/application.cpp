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
#include <mir/scene/session.h>

Application::Application(const QString &appId, Application::State state,
                         const QStringList &arguments, ApplicationManager *parent)
    : Application(new DesktopFileReader(appId), state, arguments, parent)
{
}

Application::Application(DesktopFileReader *desktopFileReader, State state,
                         const QStringList &arguments, ApplicationManager *parent)
    : ApplicationInfoInterface(desktopFileReader->appId(), parent)
    , m_appMgr(parent)
    , m_desktopData(desktopFileReader)
    , m_pid(0)
    , m_stage((m_desktopData->stageHint() == "SideStage") ? Application::SideStage : Application::MainStage)
    , m_state(state)
    , m_focused(false)
    , m_fullscreen(false)
    , m_arguments(arguments)
    , m_suspendTimer(new QTimer(this))
    , m_surface(nullptr)
{
    DLOG("Application::Application (this=%p, appId=%s, state=%d", this, qPrintable(desktopFileReader->appId()),
         static_cast<int>(state));

    m_suspendTimer->setSingleShot(true);
    connect(m_suspendTimer, SIGNAL(timeout()), this, SLOT(suspend()));

    deduceSupportedOrientationsFromAppId();
}

Application::~Application()
{
    DLOG("Application::~Application");
    delete m_desktopData;
    delete m_surface;
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

QUrl Application::screenshot() const
{
    return m_screenshot;
}

bool Application::fullscreen() const
{
    return m_fullscreen;
}

std::shared_ptr<mir::scene::Session> Application::session() const
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

void Application::setSession(const std::shared_ptr<mir::scene::Session>& session)
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
        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleStage);
    }
}

QImage Application::screenshotImage() const
{
    return m_screenshotImage;
}

void Application::updateScreenshot()
{
    if (!m_session)
        return;

    m_session->take_snapshot(
        [&](mir::scene::Snapshot const& snapshot)
        {
            DLOG("ApplicationScreenshotProvider - Mir snapshot ready with size %d x %d",
                 snapshot.size.height.as_int(), snapshot.size.width.as_int());

            m_screenshotImage = QImage( (const uchar*)snapshot.pixels, // since we mirror, no need to offset starting position
                            snapshot.size.width.as_int(),
                            snapshot.size.height.as_int(),
                            QImage::Format_ARGB32_Premultiplied).mirrored();

            m_screenshot = QString("image://application/%1/%2").arg(m_desktopData->appId()).arg(QDateTime::currentMSecsSinceEpoch());
            Q_EMIT screenshotChanged(m_screenshot);
        });
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

        // FIXME: Make this a signal-slot connection
        if (m_surface) {
            m_surface->onApplicationStateChanged();
        }
    }
}

void Application::setFocused(bool focused)
{
    DLOG("Application::setFocused (this=%p, focus=%s)", this, focused ? "yes" : "no");
    if (m_focused != focused) {
        m_focused = focused;
        Q_EMIT focusedChanged(focused);
        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleFocused);
    }
}

void Application::setFullscreen(bool fullscreen)
{
    DLOG("Application::setFullscreen (this=%p, fullscreen=%s)", this, fullscreen ? "yes" : "no");
    if (m_fullscreen != fullscreen) {
        m_fullscreen = fullscreen;
        Q_EMIT fullscreenChanged();
        QModelIndex appIndex = m_appMgr->findIndex(this);
        Q_EMIT m_appMgr->dataChanged(appIndex, appIndex, QVector<int>() << ApplicationManager::RoleFullscreen);
    }
}

void Application::suspend()
{
    DLOG("Application::suspend (this=%p, appId=%s)", this, qPrintable(appId()));
    if (m_surface) {
        m_surface->stopFrameDropper();
    } else {
        qDebug() << "Application::suspend - no surface to call stopFrameDropper() on!";
    }
    TaskController::singleton()->suspend(appId());
}

void Application::resume()
{
    DLOG("Application::resume (this=%p, appId=%s)", this, qPrintable(appId()));
    TaskController::singleton()->resume(appId());
    if (m_surface) {
        m_surface->startFrameDropper();
    }
}

void Application::respawn()
{
    DLOG("Application::respawn (this=%p)", this);
    TaskController::singleton()->start(appId(), m_arguments);
}

Application::SupportedOrientations Application::supportedOrientations() const
{
    return m_supportedOrientations;
}

void Application::deduceSupportedOrientationsFromAppId()
{
    if (appId() == "dialer-app") {
        m_supportedOrientations = PortraitOrientation;
    } else {
        m_supportedOrientations = PortraitOrientation
            | LandscapeOrientation
            | InvertedPortraitOrientation
            | InvertedLandscapeOrientation;
    }
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
    if (newSurface == m_surface) {
        return;
    }

    DLOG("Application::surface = %p", newSurface);

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
    setFullscreen(m_surface && m_surface->state() == MirSurfaceItem::Fullscreen);
}
