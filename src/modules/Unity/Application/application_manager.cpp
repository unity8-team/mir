/*
 * Copyright (C) 2013,2014 Canonical, Ltd.
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
#include "application_manager.h"
#include "application.h"
#include "desktopfilereader.h"
#include "dbuswindowstack.h"
#include "session.h"
#include "proc_info.h"
#include "taskcontroller.h"
#include "upstart/applicationcontroller.h"
#include "tracepoints.h" // generated from tracepoints.tp

// mirserver
#include "mirserverconfiguration.h"
#include "nativeinterface.h"
#include "sessionlistener.h"
#include "sessionauthorizer.h"
#include "taskcontroller.h"
#include "logging.h"

// mir
#include <mir/scene/surface.h>
#include <mir/scene/session.h>
#include <mir/graphics/display.h>
#include <mir/graphics/display_buffer.h>
#include <mir/geometry/rectangles.h>

// Qt
#include <QGuiApplication>
#include <QDebug>

// std
#include <csignal>

namespace ms = mir::scene;

Q_LOGGING_CATEGORY(QTMIR_APPLICATIONS, "qtmir.applications")

using namespace unity::shell::application;

namespace qtmir
{

namespace {

// FIXME: AppManager should not implement policy based on display geometry, shell should do that
bool forceAllAppsIntoMainStage(const QSharedPointer<MirServerConfiguration> &mirConfig)
{
    const int tabletModeMinimimWithGU = 100;

    // Obtain display size
    mir::geometry::Rectangles view_area;
    mirConfig->the_display()->for_each_display_buffer(
        [&view_area](const mir::graphics::DisplayBuffer & db)
        {
            view_area.add(db.view_area());
        });

    // Get current Grid Unit value
    int gridUnitPx = 8;
    QByteArray gridUnitString = qgetenv("GRID_UNIT_PX");
    if (!gridUnitString.isEmpty()) {
        bool ok;
        int value = gridUnitString.toInt(&ok);
        if (ok) {
            gridUnitPx = value;
        }
    }

    return (view_area.bounding_rectangle().size.width.as_int() < tabletModeMinimimWithGU * gridUnitPx);
}

// FIXME: To be removed once shell has fully adopted short appIds!!
QString toShortAppIdIfPossible(const QString &appId) {
    QRegExp longAppIdMask("[a-z0-9][a-z0-9+.-]+_[a-zA-Z0-9+.-]+_[0-9][a-zA-Z0-9.+:~-]*");
    if (longAppIdMask.exactMatch(appId)) {
        qWarning() << "WARNING: long App ID encountered:" << appId;
        // input string a long AppId, chop the version string off the end
        QStringList parts = appId.split("_");
        return QString("%1_%2").arg(parts.first()).arg(parts.at(1));
    }
    return appId;
}

void connectToSessionListener(ApplicationManager *manager, SessionListener *listener)
{
    QObject::connect(listener, &SessionListener::sessionStarting,
                     manager, &ApplicationManager::onSessionStarting);
    QObject::connect(listener, &SessionListener::sessionStopping,
                     manager, &ApplicationManager::onSessionStopping);
    QObject::connect(listener, &SessionListener::sessionCreatedSurface,
                     manager, &ApplicationManager::onSessionCreatedSurface);
}

void connectToSessionAuthorizer(ApplicationManager *manager, SessionAuthorizer *authorizer)
{
    QObject::connect(authorizer, &SessionAuthorizer::requestAuthorizationForSession,
                     manager, &ApplicationManager::authorizeSession, Qt::BlockingQueuedConnection);
}

void connectToTaskController(ApplicationManager *manager, TaskController *controller)
{
    QObject::connect(controller, &TaskController::processStarting,
                     manager, &ApplicationManager::onProcessStarting);
    QObject::connect(controller, &TaskController::processStopped,
                     manager, &ApplicationManager::onProcessStopped);
    QObject::connect(controller, &TaskController::processFailed,
                     manager, &ApplicationManager::onProcessFailed);
    QObject::connect(controller, &TaskController::requestFocus,
                     manager, &ApplicationManager::onFocusRequested);
    QObject::connect(controller, &TaskController::requestResume,
                     manager, &ApplicationManager::onResumeRequested);
}

} // namespace

ApplicationManager* ApplicationManager::Factory::Factory::create()
{
    NativeInterface *nativeInterface = dynamic_cast<NativeInterface*>(QGuiApplication::platformNativeInterface());

    if (!nativeInterface) {
        qCritical() << "ERROR: Unity.Application QML plugin requires use of the 'mirserver' QPA plugin";
        QGuiApplication::quit();
        return nullptr;
    }

    auto mirConfig = nativeInterface->m_mirConfig;

    SessionListener *sessionListener = static_cast<SessionListener*>(nativeInterface->nativeResourceForIntegration("SessionListener"));
    SessionAuthorizer *sessionAuthorizer = static_cast<SessionAuthorizer*>(nativeInterface->nativeResourceForIntegration("SessionAuthorizer"));

    QSharedPointer<upstart::ApplicationController> appController(new upstart::ApplicationController());
    QSharedPointer<TaskController> taskController(new TaskController(nullptr, appController));
    QSharedPointer<DesktopFileReader::Factory> fileReaderFactory(new DesktopFileReader::Factory());
    QSharedPointer<ProcInfo> procInfo(new ProcInfo());

    // FIXME: We should use a QSharedPointer to wrap this ApplicationManager object, which requires us
    // to use the data() method to pass the raw pointer to the QML engine. However the QML engine appears
    // to take ownership of the object, and deletes it when it wants to. This conflicts with the purpose
    // of the QSharedPointer, and a double-delete results. Trying QQmlEngine::setObjectOwnership on the
    // object no effect, which it should. Need to investigate why.
    ApplicationManager* appManager = new ApplicationManager(
                                             mirConfig,
                                             taskController,
                                             fileReaderFactory,
                                             procInfo
                                         );

    connectToSessionListener(appManager, sessionListener);
    connectToSessionAuthorizer(appManager, sessionAuthorizer);
    connectToTaskController(appManager, taskController.data());

    return appManager;
}


ApplicationManager* ApplicationManager::singleton()
{
    static ApplicationManager* instance;
    if (!instance) {
        Factory appFactory;
        instance = appFactory.create();
    }
    return instance;
}

ApplicationManager::ApplicationManager(
        const QSharedPointer<MirServerConfiguration>& mirConfig,
        const QSharedPointer<TaskController>& taskController,
        const QSharedPointer<DesktopFileReader::Factory>& desktopFileReaderFactory,
        const QSharedPointer<ProcInfo>& procInfo,
        QObject *parent)
    : ApplicationManagerInterface(parent)
    , m_mirConfig(mirConfig)
    , m_focusedApplication(nullptr)
    , m_mainStageApplication(nullptr)
    , m_sideStageApplication(nullptr)
    , m_lifecycleExceptions(QStringList() << "com.ubuntu.music")
    , m_dbusWindowStack(new DBusWindowStack(this))
    , m_taskController(taskController)
    , m_desktopFileReaderFactory(desktopFileReaderFactory)
    , m_procInfo(procInfo)
    , m_suspended(false)
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::ApplicationManager (this=%p)" << this;
    setObjectName("qtmir::ApplicationManager");

    m_roleNames.insert(RoleSession, "session");
    m_roleNames.insert(RoleFullscreen, "fullscreen");
}

ApplicationManager::~ApplicationManager()
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::~ApplicationManager";
}

int ApplicationManager::rowCount(const QModelIndex &parent) const
{
    return !parent.isValid() ? m_applications.size() : 0;
}

QVariant ApplicationManager::data(const QModelIndex &index, int role) const
{
    if (index.row() >= 0 && index.row() < m_applications.size()) {
        Application *application = m_applications.at(index.row());
        switch (role) {
            case RoleAppId:
                return QVariant::fromValue(application->appId());
            case RoleName:
                return QVariant::fromValue(application->name());
            case RoleComment:
                return QVariant::fromValue(application->comment());
            case RoleIcon:
                return QVariant::fromValue(application->icon());
            case RoleStage:
                return QVariant::fromValue((int)application->stage());
            case RoleState:
                return QVariant::fromValue((int)application->state());
            case RoleFocused:
                return QVariant::fromValue(application->focused());
            case RoleSession:
                return QVariant::fromValue(application->session());
            case RoleFullscreen:
                return QVariant::fromValue(application->fullscreen());
            default:
                return QVariant();
        }
    } else {
        return QVariant();
    }
}

Application* ApplicationManager::get(int index) const
{
    if (index < 0 || index >= m_applications.count()) {
        return nullptr;
    }
    return m_applications.at(index);
}

Application* ApplicationManager::findApplication(const QString &inputAppId) const
{
    const QString appId = toShortAppIdIfPossible(inputAppId);

    for (Application *app : m_applications) {
        if (app->appId() == appId) {
            return app;
        }
    }
    return nullptr;
}

bool ApplicationManager::requestFocusApplication(const QString &inputAppId)
{
    const QString appId = toShortAppIdIfPossible(inputAppId);

    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::requestFocusApplication - appId=" << appId;
    Application *application = findApplication(appId);

    if (!application) {
        qDebug() << "No such running application with appId=" << appId;
        return false;
    }

    Q_EMIT focusRequested(appId);
    return true;
}

QString ApplicationManager::focusedApplicationId() const
{
    if (m_focusedApplication) {
        return m_focusedApplication->appId();
    } else {
        return QString();
    }
}

bool ApplicationManager::suspended() const
{
    return m_suspended;
}

void ApplicationManager::setSuspended(bool suspended)
{
    if (suspended == m_suspended) {
        return;
    }
    m_suspended = suspended;
    Q_EMIT suspendedChanged();

    if (m_suspended) {
        suspendApplication(m_mainStageApplication);
        suspendApplication(m_sideStageApplication);
        if (m_focusedApplication) {
            m_focusedApplication->setFocused(false);
            m_dbusWindowStack->FocusedWindowChanged(0, QString(), 0);
        }
    } else {
        resumeApplication(m_mainStageApplication);
        resumeApplication(m_sideStageApplication);
        if (m_focusedApplication) {
            m_focusedApplication->setFocused(true);
            m_dbusWindowStack->FocusedWindowChanged(0, m_focusedApplication->appId(), m_focusedApplication->stage());
        }
    }
}

bool ApplicationManager::suspendApplication(Application *application)
{
    if (application == nullptr)
        return false;
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::suspendApplication - appId=" << application->appId();

    // Present in exceptions list, return.
    if (!m_lifecycleExceptions.filter(application->appId().section('_',0,0)).empty())
        return false;

    if (application->state() == Application::Running)
        application->setState(Application::Suspended);

    return true;
}

void ApplicationManager::resumeApplication(Application *application)
{
    if (application == nullptr)
        return;
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::resumeApplication - appId=" << application->appId();

    if (application->state() == Application::Suspended || application->state() == Application::Stopped)
        application->setState(Application::Running);
}

bool ApplicationManager::focusApplication(const QString &inputAppId)
{
    const QString appId = toShortAppIdIfPossible(inputAppId);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::focusApplication - appId=" << appId;
    Application *application = findApplication(appId);

    if (!application) {
        qDebug() << "No such running application with appId=" << appId;
        return false;
    }

    resumeApplication(application);

    // set state of previously focused app to suspended
    if (m_focusedApplication) {
        m_focusedApplication->setFocused(false);
        Application *lastApplication = applicationForStage(application->stage());
        suspendApplication(lastApplication);
    }

    if (application->stage() == Application::MainStage) {
        m_mainStageApplication = application;
        resumeApplication(m_sideStageApplication); // in case unfocusCurrentApplication() was last called
    } else {
        m_sideStageApplication = application;
        resumeApplication(m_mainStageApplication); // in case unfocusCurrentApplication() was last called
    }

    m_focusedApplication = application;
    m_focusedApplication->setFocused(true);

    move(m_applications.indexOf(application), 0);
    Q_EMIT focusedApplicationIdChanged();
    m_dbusWindowStack->FocusedWindowChanged(0, application->appId(), application->stage());

    // FIXME(dandrader): lying here. The operation is async. So we will only know whether
    // the focusing was successful once the server replies. Maybe the API in unity-api should
    // reflect that?
    return true;
}

void ApplicationManager::unfocusCurrentApplication()
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::unfocusCurrentApplication";

    suspendApplication(m_sideStageApplication);
    suspendApplication(m_mainStageApplication);

    m_focusedApplication = nullptr;
    Q_EMIT focusedApplicationIdChanged();
}

/**
 * @brief ApplicationManager::startApplication launches an application identified by an "application id" or appId.
 *
 * Note: due to an implementation detail, appIds come in two forms:
 * * long appId: $(click_package)_$(application)_$(version)
 * * short appId: $(click_package)_$(application)
 * It is expected that the shell uses _only_ short appIds (but long appIds are accepted by this method for legacy
 * reasons - but be warned, this ability will be removed)
 *
 * Unless stated otherwise, we always use short appIds in this API.
 *
 * @param inputAppId AppId of application to launch (long appId supported)
 * @param arguments Command line arguments to pass to the application to be launched
 * @return Pointer to Application object representing the launched process. If process already running, return nullptr
 */
Application* ApplicationManager::startApplication(const QString &appId,
                                                  const QStringList &arguments)
{
    return startApplication(appId, NoFlag, arguments);
}

Application *ApplicationManager::startApplication(const QString &inputAppId, ExecFlags flags,
                                                  const QStringList &arguments)
{
    tracepoint(qtmir, startApplication);
    QString appId = toShortAppIdIfPossible(inputAppId);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::startApplication - this=" << this << "appId" << qPrintable(appId);

    Application *application = findApplication(appId);
    if (application) {
        qWarning() << "ApplicationManager::startApplication - application appId=" << appId << " already exists";
        return nullptr;
    }

    if (!m_taskController->start(appId, arguments)) {
        qWarning() << "Upstart failed to start application with appId" << appId;
        return nullptr;
    }

    // The TaskController may synchroneously callback onProcessStarting, so check if application already added
    application = findApplication(appId);
    if (application) {
        application->setArguments(arguments);
    } else {
        application = new Application(
                    m_taskController,
                    m_desktopFileReaderFactory->createInstance(appId, m_taskController->findDesktopFileForAppId(appId)),
                    Application::Starting,
                    arguments,
                    this);

        if (!application->isValid()) {
            qWarning() << "Unable to instantiate application with appId" << appId;
            return nullptr;
        }

        // override stage if necessary
        if (application->stage() == Application::SideStage && flags.testFlag(ApplicationManager::ForceMainStage)) {
            application->setStage(Application::MainStage);
        }

        add(application);
    }
    return application;
}

void ApplicationManager::onProcessStarting(const QString &appId)
{
    tracepoint(qtmir, onProcessStarting);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onProcessStarting - appId=" << appId;

    Application *application = findApplication(appId);
    if (!application) { // then shell did not start this application, so ubuntu-app-launch must have - add to list
        application = new Application(
                    m_taskController,
                    m_desktopFileReaderFactory->createInstance(appId, m_taskController->findDesktopFileForAppId(appId)),
                    Application::Starting,
                    QStringList(),
                    this);

        if (!application->isValid()) {
            qWarning() << "Unable to instantiate application with appId" << appId;
            return;
        }

        // override stage if necessary (i.e. side stage invalid on phone)
        if (application->stage() == Application::SideStage && forceAllAppsIntoMainStage(m_mirConfig))
            application->setStage(Application::MainStage);

        add(application);
        Q_EMIT focusRequested(appId);
    }
    else {
        // url-dispatcher can relaunch apps which have been OOM-killed - AppMan must accept the newly spawned
        // application and focus it immediately (as user expects app to still be running).
        if (application->state() == Application::Stopped) {
            qCDebug(QTMIR_APPLICATIONS) << "Stopped application appId=" << appId << "is being resumed externally";
            application->setState(Application::Starting);
            Q_EMIT focusRequested(appId);
        } else {
            qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onProcessStarting application already found with appId"
                                        << appId;
        }
    }
}

/**
 * @brief ApplicationManager::stopApplication - stop a running application and remove from list
 * @param inputAppId
 * @return True if running application was stopped, false if application did not exist or could not be stopped
 */
bool ApplicationManager::stopApplication(const QString &inputAppId)
{
    const QString appId = toShortAppIdIfPossible(inputAppId);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::stopApplication - appId=" << appId;

    Application *application = findApplication(appId);
    if (!application) {
        qCritical() << "No such running application with appId" << appId;
        return false;
    }

    if (application == m_focusedApplication) {
        // unfocus, and let shell decide what next to focus
        m_focusedApplication = nullptr;
        Q_EMIT focusedApplicationIdChanged();
    }

    remove(application);
    m_dbusWindowStack->WindowDestroyed(0, appId);

    bool result = m_taskController->stop(application->longAppId());

    if (!result && application->pid() > 0) {
        qWarning() << "FAILED to ask Upstart to stop application with appId" << appId
                   << "Sending SIGTERM to process:" << application->pid();
        kill(application->pid(), SIGTERM);
        result = true;
    }

    delete application;
    return result;
}

void ApplicationManager::onProcessFailed(const QString &appId, const bool duringStartup)
{
    /* Applications fail if they fail to launch, crash or are killed. If failed to start, must
     * immediately remove from list of applications. If crash or kill, instead we set flag on the
     * Application to indicate it can be resumed.
     */

    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onProcessFailed - appId=" << appId << "duringStartup=" << duringStartup;

    Application *application = findApplication(appId);
    if (!application) {
        qWarning() << "ApplicationManager::onProcessFailed - upstart reports failure of application" << appId
                   << "that AppManager is not managing";
        return;
    }

    Q_UNUSED(duringStartup); // FIXME(greyback) upstart reports app that fully started up & crashes as failing during startup??
    if (application->state() == Application::Starting) {
        if (application == m_focusedApplication) {
            m_focusedApplication = nullptr;
            Q_EMIT focusedApplicationIdChanged();
        }
        remove(application);
        m_dbusWindowStack->WindowDestroyed(0, application->appId());
        delete application;
    } else {
        // We need to set flags on the Application to say the app can be resumed, and thus should not be removed
        // from the list by onProcessStopped.
        application->setCanBeResumed(true);
        application->setPid(0);
    }
}

void ApplicationManager::onProcessStopped(const QString &appId)
{
    tracepoint(qtmir, onProcessStopped);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onProcessStopped - appId=" << appId;
    Application *application = findApplication(appId);

    if (!application) {
        qDebug() << "ApplicationManager::onProcessStopped reports stop of appId=" << appId
                 << "which AppMan is not managing, ignoring the event";
        return;
    }

    // if shell did not stop the application, but ubuntu-app-launch says it died, we assume the process has been
    // killed, so it can be respawned later. Only exception is if that application is focused or running
    // as then it most likely crashed. Update this logic when ubuntu-app-launch gives some failure info.
    bool removeApplication = true;

    if (application == m_focusedApplication) {
        // Very bad case where focused application dies. Remove from list. Should give error message
        m_focusedApplication = nullptr;
        Q_EMIT focusedApplicationIdChanged();
    }

    // The following scenario is the only time that we do NOT remove the application from the app list:
    if ((application->state() == Application::Suspended || application->state() == Application::Stopped)
            && application->pid() == 0 // i.e. onProcessFailed was called, which resets the PID of this application
            && application->canBeResumed()) {
        removeApplication = false;
    }

    if (removeApplication) {
        qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onProcessStopped - removing appId=" << appId;
        remove(application);
        m_dbusWindowStack->WindowDestroyed(0, application->appId());
        delete application;
    }
}

void ApplicationManager::onFocusRequested(const QString& appId)
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onFocusRequested - appId=" << appId;

    Q_EMIT focusRequested(appId);
}

void ApplicationManager::onResumeRequested(const QString& appId)
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onResumeRequested - appId=" << appId;

    Application *application = findApplication(appId);

    if (!application) {
        qCritical() << "ApplicationManager::onResumeRequested: No such running application" << appId;
        return;
    }

    // If app Stopped, trust that ubuntu-app-launch respawns it itself, and AppManager will
    // be notified of that through the onProcessStartReportReceived slot. Else resume.
    if (application->state() == Application::Suspended) {
        application->setState(Application::Running);
    }
}

void ApplicationManager::onAppDataChanged(const int role)
{
    if (sender()) {
        Application *application = static_cast<Application*>(sender());
        QModelIndex appIndex = findIndex(application);
        Q_EMIT dataChanged(appIndex, appIndex, QVector<int>() << role);

        qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onAppDataChanged: Received " << m_roleNames[role] << " update", application->appId();
    } else {
        qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onAppDataChanged: Received " << m_roleNames[role] << " signal but application has disappeard.";
    }
}

void ApplicationManager::authorizeSession(const quint64 pid, bool &authorized)
{
    tracepoint(qtmir, authorizeSession);
    authorized = false; //to be proven wrong

    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::authorizeSession - pid=" << pid;

    for (Application *app : m_applications) {
        if (app->state() == Application::Starting) {
            tracepoint(qtmir, appIdHasProcessId_start);
            if (m_taskController->appIdHasProcessId(app->appId(), pid)) {
                app->setPid(pid);
                authorized = true;
                tracepoint(qtmir, appIdHasProcessId_end, 1); //found
                return;
            }
            tracepoint(qtmir, appIdHasProcessId_end, 0); // not found
        }
    }

    /*
     * Hack: Allow applications to be launched without being managed by upstart, where AppManager
     * itself manages processes executed with a "--desktop_file_hint=/path/to/desktopFile.desktop"
     * parameter attached. This exists until ubuntu-app-launch can notify shell any application is
     * and so shell should allow it. Also reads the --stage parameter to determine the desired stage
     */
    std::unique_ptr<ProcInfo::CommandLine> info = m_procInfo->commandLine(pid);
    if (!info) {
        qWarning() << "ApplicationManager REJECTED connection from app with pid" << pid
                   << "as unable to read the process command line";
        return;
    }

    if (info->startsWith("maliit-server") || info->contains("qt5/libexec/QtWebProcess")) {
        authorized = true;
        m_hiddenPIDs << pid;
        return;
    }

    boost::optional<QString> desktopFileName{ info->getParameter("--desktop_file_hint=") };

    if (!desktopFileName) {
        qCritical() << "ApplicationManager REJECTED connection from app with pid" << pid
                    << "as no desktop_file_hint specified";
        return;
    }

    qCDebug(QTMIR_APPLICATIONS) << "Process supplied desktop_file_hint, loading:" << desktopFileName;

    // Guess appId from the desktop file hint
    QString appId = toShortAppIdIfPossible(desktopFileName.get().remove(QRegExp(".desktop$")).split('/').last());

    // FIXME: right now we support --desktop_file_hint=appId for historical reasons. So let's try that in
    // case we didn't get an existing .desktop file path
    DesktopFileReader* desktopData;
    if (QFileInfo::exists(desktopFileName.get())) {
        desktopData = m_desktopFileReaderFactory->createInstance(appId, QFileInfo(desktopFileName.get()));
    } else {
        qCDebug(QTMIR_APPLICATIONS) << "Unable to find file:" << desktopFileName.get()
                                    << "so will search standard paths for one named" << appId << ".desktop";
        desktopData = m_desktopFileReaderFactory->createInstance(appId, m_taskController->findDesktopFileForAppId(appId));
    }

    if (!desktopData->loaded()) {
        delete desktopData;
        qCritical() << "ApplicationManager REJECTED connection from app with pid" << pid
                    << "as the file specified by the desktop_file_hint argument could not be opened";
        return;
    }

    // some naughty applications use a script to launch the actual application. Check for the
    // case where shell actually launched the script.
    Application *application = findApplication(desktopData->appId());
    if (application && application->state() == Application::Starting) {
        qCDebug(QTMIR_APPLICATIONS) << "Process with pid" << pid << "appeared, attaching to existing entry"
                                    << "in application list with appId:" << application->appId();
        delete desktopData;
        application->setPid(pid);
        authorized = true;
        return;
    }

    // if stage supplied in CLI, fetch that
    Application::Stage stage = Application::MainStage;
    boost::optional<QString> stageParam = info->getParameter("--stage_hint=");

    if (stageParam && stageParam.get() == "side_stage") {
        stage = Application::SideStage;
    }

    qCDebug(QTMIR_APPLICATIONS) << "New process with pid" << pid << "appeared, adding new application to the"
                                << "application list with appId:" << desktopData->appId();

    QStringList arguments(info->asStringList());
    application = new Application(
        m_taskController,
        desktopData,
        Application::Starting,
        arguments,
        this);
    application->setPid(pid);
    application->setStage(stage);
    application->setCanBeResumed(false);
    add(application);
    authorized = true;
}

void ApplicationManager::onSessionStarting(std::shared_ptr<ms::Session> const& session)
{
    Q_UNUSED(session);
}

void ApplicationManager::onSessionStopping(std::shared_ptr<ms::Session> const& session)
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onSessionStopping - sessionName=" << session->name().c_str();

    // in case application closed not by hand of shell, check again here:
    Application* application = findApplicationWithSession(session);
    if (application) {
        /* Can remove the application from the running apps list immediately in these curcumstances:
         *  1. application is not managed by upstart (this message from Mir is only notice the app has stopped, must do
         *     it here)
         *  2. application is managed by upstart, but has stopped before it managed to create a surface, we can assume
         *     it crashed on startup, and thus cannot be resumed - so remove it.
         *  3. application is managed by upstart and is in foreground (i.e. has Running state), if Mir reports the
         *     application disconnects, it either crashed or stopped itself. Either case, remove it.
         */
        if (!application->canBeResumed()
                || application->state() == Application::Starting
                || application->state() == Application::Running) {
            m_dbusWindowStack->WindowDestroyed(0, application->appId());
            remove(application);
            delete application;

            if (application == m_focusedApplication) {
                m_focusedApplication = nullptr;
                Q_EMIT focusedApplicationIdChanged();
            }
        } else {
            // otherwise, we do not have enough information to make any changes to the model, so await events from
            // upstart to go further, but set the app state
            application->setState(Application::Stopped);
        }
    }
    m_hiddenPIDs.removeOne(session->process_id());
}

void ApplicationManager::onSessionCreatedSurface(ms::Session const* session,
                                               std::shared_ptr<ms::Surface> const& surface)
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::onSessionCreatedSurface - sessionName=" << session->name().c_str();
    Q_UNUSED(surface);

    Application* application = findApplicationWithSession(session);
    if (application && application->state() == Application::Starting) {
        m_dbusWindowStack->WindowCreated(0, application->appId());
        application->setState(Application::Running);
    }
}

Application* ApplicationManager::findApplicationWithSession(const std::shared_ptr<ms::Session> &session)
{
    return findApplicationWithSession(session.get());
}

Application* ApplicationManager::findApplicationWithSession(const ms::Session *session)
{
    if (!session)
        return nullptr;
    return findApplicationWithPid(session->process_id());
}

Application* ApplicationManager::findApplicationWithPid(const qint64 pid)
{
    if (pid <= 0)
        return nullptr;

    for (Application *app : m_applications) {
        if (app->m_pid == pid) {
            return app;
        }
    }
    return nullptr;
}

Application* ApplicationManager::applicationForStage(Application::Stage stage)
{
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::focusedApplicationForStage" << stage;

    if (stage == Application::MainStage)
        return m_mainStageApplication;
    else
        return m_sideStageApplication;
}

void ApplicationManager::add(Application* application)
{
    Q_ASSERT(application != nullptr);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::add - appId=" << application->appId();

    connect(application, &Application::fullscreenChanged, this, [this](bool) { onAppDataChanged(RoleFullscreen); });
    connect(application, &Application::focusedChanged, this, [this](bool) { onAppDataChanged(RoleFocused); });
    connect(application, &Application::stateChanged, this, [this](Application::State) { onAppDataChanged(RoleState); });
    connect(application, &Application::stageChanged, this, [this](Application::Stage) { onAppDataChanged(RoleStage); });

    beginInsertRows(QModelIndex(), m_applications.count(), m_applications.count());
    m_applications.append(application);
    endInsertRows();
    Q_EMIT countChanged();
    Q_EMIT applicationAdded(application->appId());
    if (m_applications.size() == 1) {
        Q_EMIT emptyChanged();
    }
}

void ApplicationManager::remove(Application *application)
{
    Q_ASSERT(application != nullptr);
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::remove - appId=" << application->appId();

    if (application == m_sideStageApplication)
        m_sideStageApplication = nullptr;
    if (application == m_mainStageApplication)
        m_mainStageApplication = nullptr;

    application->disconnect(this);

    int i = m_applications.indexOf(application);
    if (i != -1) {
        beginRemoveRows(QModelIndex(), i, i);
        m_applications.removeAt(i);
        endRemoveRows();
        Q_EMIT applicationRemoved(application->appId());
        Q_EMIT countChanged();
        if (i == 0) {
            Q_EMIT emptyChanged();
        }
    }
}

void ApplicationManager::move(int from, int to) {
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::move - from=" << from << "to=" << to;
    if (from == to) return;

    if (from >= 0 && from < m_applications.size() && to >= 0 && to < m_applications.size()) {
        QModelIndex parent;
        /* When moving an item down, the destination index needs to be incremented
           by one, as explained in the documentation:
           http://qt-project.org/doc/qt-5.0/qtcore/qabstractitemmodel.html#beginMoveRows */

        beginMoveRows(parent, from, from, parent, to + (to > from ? 1 : 0));
        m_applications.move(from, to);
        endMoveRows();
    }
    qCDebug(QTMIR_APPLICATIONS) << "ApplicationManager::move after " << toString();
}

QModelIndex ApplicationManager::findIndex(Application* application)
{
    for (int i = 0; i < m_applications.size(); ++i) {
        if (m_applications.at(i) == application) {
            return index(i);
        }
    }

    return QModelIndex();
}

QString ApplicationManager::toString() const
{
    QString result;
    for (int i = 0; i < m_applications.count(); ++i) {
        if (i > 0) {
            result.append(",");
        }
        result.append(m_applications.at(i)->appId());
    }
    return result;
}

} // namespace qtmir
