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
#include "debughelpers.h"
#include "desktopfilereader.h"
#include "mirsessionitem.h"
#include "taskcontroller.h"

// QPA mirserver
#include "logging.h"

// mir
#include <mir/scene/session.h>
#include <mir/scene/snapshot.h>
#include <mir/scene/prompt_session_manager.h>

namespace ms = mir::scene;

namespace qtmir
{

Application::Application(const QSharedPointer<TaskController>& taskController,
                         DesktopFileReader *desktopFileReader,
                         State state,
                         const QStringList &arguments,
                         const std::shared_ptr<ms::PromptSessionManager>& promptSessionManager,
                         ApplicationManager *parent)
    : ApplicationInfoInterface(desktopFileReader->appId(), parent)
    , m_taskController(taskController)
    , m_desktopData(desktopFileReader)
    , m_pid(0)
    , m_stage((m_desktopData->stageHint() == "SideStage") ? Application::SideStage : Application::MainStage)
    , m_state(state)
    , m_focused(false)
    , m_canBeResumed(true)
    , m_arguments(arguments)
    , m_session(nullptr)
    , m_promptSessionManager(promptSessionManager)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::Application - appId=" << desktopFileReader->appId() << "state=" << state;

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
    qCDebug(QTMIR_APPLICATIONS) << "Application::~Application";

    stopPromptSessions();
    delete m_session;
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

QUrl Application::screenshot() const
{
    return m_screenshot;
}

bool Application::fullscreen() const
{
    return m_session ? m_session->fullscreen() : false;
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

void Application::setSession(MirSessionItem *newSession)
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::setSession - appId=" << appId() << "session=" << newSession;

    if (newSession == m_session)
        return;

    if (m_session) {
        m_session->disconnect(this);
    }

    bool oldFullscreen = fullscreen();
    m_session = newSession;

    if (m_session) {
        m_session->setParent(this);
        m_session->setApplication(this);

        connect(m_session, &MirSessionItem::suspend, this, &Application::suspend);
        connect(m_session, &MirSessionItem::resume, this, &Application::resume);
        connect(m_session, &MirSessionItem::respawn, this, &Application::respawn);
        connect(m_session, &MirSessionItem::fullscreenChanged, this, &Application::fullscreenChanged);

        if (oldFullscreen != fullscreen())
            Q_EMIT fullscreenChanged(fullscreen());
    }

    Q_EMIT sessionChanged(m_session);
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
    }
}

QImage Application::screenshotImage() const
{
    return m_screenshotImage;
}

void Application::updateScreenshot()
{
    if (!session())
        return;

    session()->takeSnapshot(
        [this](mir::scene::Snapshot const& snapshot)
        {
            qCDebug(QTMIR_APPLICATIONS) << "ApplicationScreenshotProvider - Mir snapshot ready with size"
                                        << snapshot.size.height.as_int() << "x" << snapshot.size.width.as_int();

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
    qCDebug(QTMIR_APPLICATIONS) << "Application::setState - appId=" << appId() << "state=" << applicationStateToStr(state);
    if (m_state != state) {
        switch (state)
        {
        case Application::Starting:
            if (session()) session()->setState(MirSessionItem::State::Starting);
            break;
        case Application::Suspended:
            if (session()) session()->setState(MirSessionItem::State::Suspended);
            break;
        case Application::Running:
            if (session()) session()->setState(MirSessionItem::State::Running);
            break;
        case Application::Stopped:
            if (session()) session()->setState(MirSessionItem::State::Stopped);
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
    qCDebug(QTMIR_APPLICATIONS) << "Application::setFocused - appId=" << appId() << "focused=" << focused;
    if (m_focused != focused) {
        m_focused = focused;
        Q_EMIT focusedChanged(focused);
    }
}

void Application::suspend()
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::suspend - appId=" << appId();
    m_taskController->suspend(longAppId());
}

void Application::resume()
{
    qCDebug(QTMIR_APPLICATIONS) << "Application::resume - appId=" << appId();
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

MirSessionItem* Application::session() const
{
    return m_session;
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

} // namespace qtmir
