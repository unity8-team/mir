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
 *
 */

#include <thread>
#include <condition_variable>
#include <QSignalSpy>

#include <applicationscreenshotprovider.h>

 #include "mock_surface.h"
 #include "qtmir_test.h"

using namespace qtmir;

namespace ms = mir::scene;

class ApplicationManagerTests : public ::testing::QtMirTest
{
public:
    ApplicationManagerTests()
    {}

    inline void onSessionStarting(const std::shared_ptr<mir::scene::Session> &session) {
        applicationManager.onSessionStarting(session);
        sessionManager.onSessionStarting(session);
    }
    inline void onSessionStopping(const std::shared_ptr<mir::scene::Session> &session) {
        applicationManager.onSessionStopping(session);
        sessionManager.onSessionStopping(session);
    }
};

TEST_F(ApplicationManagerTests, SuspendingAndResumingARunningApplicationResultsInOomScoreAdjustment)
{
    using namespace ::testing;

    const QString appId("com.canonical.does.not.exist");

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(_, _)).Times(1);
    EXPECT_CALL(appController, findDesktopFileForAppId(appId)).Times(1);

    EXPECT_CALL(desktopFileReaderFactory, createInstance(_, _)).Times(1);

    EXPECT_CALL(processController, sigStopProcessGroupForPid(_)).Times(1);
    EXPECT_CALL(processController, sigContinueProcessGroupForPid(_)).Times(1);
    EXPECT_CALL(oomController, ensureProcessUnlikelyToBeKilled(_)).Times(1);
    EXPECT_CALL(oomController, ensureProcessLikelyToBeKilled(_)).Times(1);

    auto application = applicationManager.startApplication(
                appId,
                ApplicationManager::NoFlag,
                QStringList());

    application->suspend();
    application->resume();
}

// Currently disabled as we need to make sure that we have a corresponding mir session, too.
TEST_F(ApplicationManagerTests, DISABLED_FocusingRunningApplicationResultsInOomScoreAdjustment)
{
    using namespace ::testing;

    const QString appId("com.canonical.does.not.exist");

    QSet<QString> appIds;

    for (unsigned int i = 0; i < 50; i++)
    {
        QString appIdFormat("%1.does.not.exist");
        auto appId = appIdFormat.arg(i);

        auto application = applicationManager.startApplication(
                    appId,
                    ApplicationManager::NoFlag,
                    QStringList());

        std::shared_ptr<mir::scene::Session> mirSession = std::make_shared<MockSession>(appIdFormat.toStdString(), i);
        onSessionStarting( mirSession );

        EXPECT_NE(nullptr, application);

        appIds.insert(appId);
        auto it = appController.children.find(appId);
        if (it != appController.children.end())
            EXPECT_CALL(oomController,
                        ensureProcessUnlikelyToBeKilled(it->pid())).Times(1);
    }

    for (auto appId : appIds)
    {
        applicationManager.focusApplication(appId);
    }
}

TEST_F(ApplicationManagerTests,bug_case_1240400_second_dialer_app_fails_to_authorize_and_gets_mixed_up_with_first_one)
{
    using namespace ::testing;
    std::shared_ptr<mir::scene::Surface> aSurface(nullptr);
    quint64 firstProcId = 5921;
    quint64 secondProcId = 5922;
    const char dialer_app_id[] = "dialer-app";
    QByteArray cmdLine( "/usr/bin/dialer-app --desktop_file_hint=dialer-app");
    QByteArray secondcmdLine( "/usr/bin/dialer-app");

    EXPECT_CALL(procInfo,command_line(firstProcId))
        .Times(1)
        .WillOnce(Return(cmdLine));
    EXPECT_CALL(procInfo,command_line(secondProcId))
        .Times(1)
        .WillOnce(Return(secondcmdLine));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> mirSession = std::make_shared<MockSession>(dialer_app_id, firstProcId);
    applicationManager.authorizeSession(firstProcId, authed);
    EXPECT_EQ(true, authed);
    onSessionStarting(mirSession);
    applicationManager.onSessionCreatedSurface(mirSession.get(),aSurface);
    Application * app = applicationManager.findApplication(dialer_app_id);
    EXPECT_NE(nullptr,app);

    // now a second session without desktop file is launched:
    applicationManager.authorizeSession(secondProcId, authed);
    applicationManager.onProcessStarting(dialer_app_id);

    EXPECT_EQ(false,authed);
    EXPECT_EQ(app,applicationManager.findApplication(dialer_app_id));
}

TEST_F(ApplicationManagerTests,application_dies_while_starting)
{
    using namespace ::testing;
    quint64 procId = 5921;
    const char app_id[] = "my-app";
    QByteArray cmdLine( "/usr/bin/my-app --desktop_file_hint=my-app");

    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> mirSession = std::make_shared<MockSession>(app_id, procId);
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(mirSession);
    Application * beforeFailure = applicationManager.findApplication(app_id);
    applicationManager.onProcessStarting(app_id);
    applicationManager.onProcessFailed(app_id, true);
    Application * afterFailure = applicationManager.findApplication(app_id);

    EXPECT_EQ(true, authed);
    EXPECT_NE(nullptr, beforeFailure);
    EXPECT_EQ(nullptr, afterFailure);
}

TEST_F(ApplicationManagerTests,application_start_failure_after_starting)
{
    using namespace ::testing;
    quint64 procId = 5921;
    std::shared_ptr<mir::scene::Surface> aSurface(nullptr);
    const char app_id[] = "my-app";
    QByteArray cmdLine( "/usr/bin/my-app --desktop_file_hint=my-app");

    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> mirSession = std::make_shared<MockSession>(app_id, procId);
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(mirSession);
    Application * beforeFailure = applicationManager.findApplication(app_id);
    applicationManager.onSessionCreatedSurface(mirSession.get(), aSurface);
    applicationManager.onProcessStarting(app_id);
    applicationManager.onProcessFailed(app_id, false);
    Application * afterFailure = applicationManager.findApplication(app_id);

    EXPECT_EQ(true, authed);
    EXPECT_NE(nullptr, beforeFailure);
    EXPECT_EQ(beforeFailure, afterFailure);
}

TEST_F(ApplicationManagerTests,startApplicationSupportsShortAppId)
{
    using namespace ::testing;

    const QString shortAppId("com.canonical.test_test");

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(_, _)).Times(1);
    EXPECT_CALL(appController, findDesktopFileForAppId(shortAppId)).Times(1);

    EXPECT_CALL(desktopFileReaderFactory, createInstance(_, _)).Times(1);

    auto application = applicationManager.startApplication(
                shortAppId,
                ApplicationManager::NoFlag,
                QStringList());

    EXPECT_EQ(shortAppId, application->appId());
}

TEST_F(ApplicationManagerTests,startApplicationSupportsLongAppId)
{
    using namespace ::testing;

    const QString longAppId("com.canonical.test_test_0.1.235");
    const QString shortAppId("com.canonical.test_test");

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(_, _)).Times(1);
    EXPECT_CALL(appController, findDesktopFileForAppId(shortAppId)).Times(1);

    EXPECT_CALL(desktopFileReaderFactory, createInstance(_, _)).Times(1);

    auto application = applicationManager.startApplication(
                longAppId,
                ApplicationManager::NoFlag,
                QStringList());

    EXPECT_EQ(shortAppId, application->appId());
}

TEST_F(ApplicationManagerTests,testAppIdGuessFromDesktopFileName)
{
    using namespace ::testing;
    quint64 procId = 5921;
    QString appId("sudoku-app");
    QString cmdLine = QString("/usr/bin/my-app --desktop_file_hint=/usr/share/click/preinstalled/com.ubuntu.sudoku/1.0.180/%1.desktop").arg(appId);

    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(qPrintable(cmdLine)));

    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    Application *app = applicationManager.findApplication(appId);

    EXPECT_EQ(true, authed);
    EXPECT_NE(app, nullptr);
    EXPECT_EQ(appId, app->appId());
}

TEST_F(ApplicationManagerTests,testAppIdGuessFromDesktopFileNameWithLongAppId)
{
    using namespace ::testing;
    quint64 procId = 5921;
    QString shortAppId("com.ubuntu.desktop_desktop");
    QString cmdLine = QString("/usr/bin/my-app --desktop_file_hint=/usr/share/applications/%1_1.0.180.desktop").arg(shortAppId);

    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(qPrintable(cmdLine)));

    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    Application *app = applicationManager.findApplication(shortAppId);

    EXPECT_EQ(true, authed);
    EXPECT_NE(app, nullptr);
    EXPECT_EQ(shortAppId, app->appId());
}

TEST_F(ApplicationManagerTests,bug_case_1281075_session_ptrs_always_distributed_to_last_started_app)
{
    using namespace ::testing;
    quint64 first_procId = 5921;
    quint64 second_procId = 5922;
    quint64 third_procId = 5923;
    std::shared_ptr<mir::scene::Surface> aSurface(nullptr);
    const char first_app_id[] = "app1";
    QByteArray first_cmdLine( "/usr/bin/app1 --desktop_file_hint=app1");
    const char second_app_id[] = "app2";
    QByteArray second_cmdLine( "/usr/bin/app2--desktop_file_hint=app2");
    const char third_app_id[] = "app3";
    QByteArray third_cmdLine( "/usr/bin/app3 --desktop_file_hint=app3");

    EXPECT_CALL(procInfo,command_line(first_procId))
        .Times(1)
        .WillOnce(Return(first_cmdLine));

    ON_CALL(appController,appIdHasProcessId(_,_)).WillByDefault(Return(false));

    EXPECT_CALL(procInfo,command_line(second_procId))
        .Times(1)
        .WillOnce(Return(second_cmdLine));

    EXPECT_CALL(procInfo,command_line(third_procId))
        .Times(1)
        .WillOnce(Return(third_cmdLine));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> first_session = std::make_shared<MockSession>("Oo", first_procId);
    std::shared_ptr<mir::scene::Session> second_session = std::make_shared<MockSession>("oO", second_procId);
    std::shared_ptr<mir::scene::Session> third_session = std::make_shared<MockSession>("OO", third_procId);
    applicationManager.authorizeSession(first_procId, authed);
    applicationManager.authorizeSession(second_procId, authed);
    applicationManager.authorizeSession(third_procId, authed);
    onSessionStarting(first_session);
    onSessionStarting(third_session);
    onSessionStarting(second_session);

    Application * firstApp = applicationManager.findApplication(first_app_id);
    Application * secondApp = applicationManager.findApplication(second_app_id);
    Application * thirdApp = applicationManager.findApplication(third_app_id);

    EXPECT_EQ(first_session, firstApp->session()->session());
    EXPECT_EQ(second_session, secondApp->session()->session());
    EXPECT_EQ(third_session, thirdApp->session()->session());
}

TEST_F(ApplicationManagerTests,two_session_on_one_application)
{
    using namespace ::testing;
    quint64 a_procId = 5921;
    const char an_app_id[] = "some_app";
    QByteArray a_cmd( "/usr/bin/app1 --desktop_file_hint=some_app");

    ON_CALL(procInfo,command_line(_)).WillByDefault(Return(a_cmd));

    ON_CALL(appController,appIdHasProcessId(_,_)).WillByDefault(Return(false));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> first_session = std::make_shared<MockSession>("Oo", a_procId);
    std::shared_ptr<mir::scene::Session> second_session = std::make_shared<MockSession>("oO", a_procId);
    applicationManager.authorizeSession(a_procId, authed);

    onSessionStarting(first_session);
    onSessionStarting(second_session);

    Application * the_app = applicationManager.findApplication(an_app_id);

    EXPECT_EQ(true, authed);
    EXPECT_EQ(second_session, the_app->session()->session());
}

TEST_F(ApplicationManagerTests,DISABLED_upstart_launching_sidestage_app_on_phone_forced_into_mainstage)
{
    using namespace ::testing;
    QString appId("sideStage");

    EXPECT_CALL(appController, findDesktopFileForAppId(appId)).Times(1);

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, stageHint()).WillByDefault(Return("SideStage"));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    // mock upstart launching an app which reports itself as sidestage, but we're on phone
    applicationManager.onProcessStarting(appId);

    // ensure the app stage is overridden to be main stage
    Application* theApp = applicationManager.findApplication(appId);
    ASSERT_NE(theApp, nullptr);
    EXPECT_EQ(Application::MainStage, theApp->stage());
}

TEST_F(ApplicationManagerTests,two_session_on_one_application_after_starting)
{
    using namespace ::testing;
    quint64 a_procId = 5921;
    const char an_app_id[] = "some_app";
    QByteArray a_cmd( "/usr/bin/app1 --desktop_file_hint=some_app");
    std::shared_ptr<mir::scene::Surface> aSurface(nullptr);

    ON_CALL(procInfo,command_line(_)).WillByDefault(Return(a_cmd));

    ON_CALL(appController,appIdHasProcessId(_,_)).WillByDefault(Return(false));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> first_session = std::make_shared<MockSession>("Oo", a_procId);
    std::shared_ptr<mir::scene::Session> second_session = std::make_shared<MockSession>("oO", a_procId);
    applicationManager.authorizeSession(a_procId, authed);

    onSessionStarting(first_session);
    applicationManager.onSessionCreatedSurface(first_session.get(), aSurface);
    onSessionStarting(second_session);

    Application * the_app = applicationManager.findApplication(an_app_id);

    EXPECT_EQ(true, authed);
    EXPECT_EQ(Application::Running, the_app->state());
    EXPECT_EQ(first_session, the_app->session()->session());
}

TEST_F(ApplicationManagerTests,suspended_suspends_focused_app_and_marks_it_unfocused_in_the_model)
{
    using namespace ::testing;
    quint64 a_procId = 5921;
    const char an_app_id[] = "some_app";
    QByteArray a_cmd( "/usr/bin/app1 --desktop_file_hint=some_app");
    std::shared_ptr<mir::scene::Surface> aSurface(nullptr);

    ON_CALL(procInfo,command_line(_)).WillByDefault(Return(a_cmd));

    ON_CALL(appController,appIdHasProcessId(_,_)).WillByDefault(Return(false));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> first_session = std::make_shared<MockSession>("Oo", a_procId);
    std::shared_ptr<mir::scene::Session> second_session = std::make_shared<MockSession>("oO", a_procId);
    applicationManager.authorizeSession(a_procId, authed);

    onSessionStarting(first_session);
    applicationManager.onSessionCreatedSurface(first_session.get(), aSurface);
    onSessionStarting(second_session);

    Application * the_app = applicationManager.findApplication(an_app_id);
    applicationManager.focusApplication(an_app_id);

    EXPECT_EQ(Application::Running, the_app->state());

    applicationManager.setSuspended(true);

    EXPECT_EQ(Application::Suspended, the_app->state());
    EXPECT_EQ(false, the_app->focused());

    applicationManager.setSuspended(false);

    EXPECT_EQ(Application::Running, the_app->state());
    EXPECT_EQ(true, the_app->focused());
}

TEST_F(ApplicationManagerTests,requestFocusApplication)
{
    using namespace ::testing;
    quint64 first_procId = 5921;
    quint64 second_procId = 5922;
    quint64 third_procId = 5923;
    std::shared_ptr<mir::scene::Surface> aSurface(nullptr);
    QByteArray first_cmdLine( "/usr/bin/app1 --desktop_file_hint=app1");
    QByteArray second_cmdLine( "/usr/bin/app2--desktop_file_hint=app2");
    QByteArray third_cmdLine( "/usr/bin/app3 --desktop_file_hint=app3");

    EXPECT_CALL(procInfo,command_line(first_procId))
        .Times(1)
        .WillOnce(Return(first_cmdLine));

    ON_CALL(appController,appIdHasProcessId(_,_)).WillByDefault(Return(false));

    EXPECT_CALL(procInfo,command_line(second_procId))
        .Times(1)
        .WillOnce(Return(second_cmdLine));

    EXPECT_CALL(procInfo,command_line(third_procId))
        .Times(1)
        .WillOnce(Return(third_cmdLine));

    bool authed = true;

    std::shared_ptr<mir::scene::Session> first_session = std::make_shared<MockSession>("Oo", first_procId);
    std::shared_ptr<mir::scene::Session> second_session = std::make_shared<MockSession>("oO", second_procId);
    std::shared_ptr<mir::scene::Session> third_session = std::make_shared<MockSession>("OO", third_procId);
    applicationManager.authorizeSession(first_procId, authed);
    applicationManager.authorizeSession(second_procId, authed);
    applicationManager.authorizeSession(third_procId, authed);
    onSessionStarting(first_session);
    onSessionStarting(third_session);
    onSessionStarting(second_session);

    QSignalSpy spy(&applicationManager, SIGNAL(focusRequested(const QString &)));

    applicationManager.requestFocusApplication("app3");

    EXPECT_EQ(spy.count(), 1);

    QList<QVariant> arguments = spy.takeFirst(); // take the first signal
    EXPECT_EQ(arguments.at(0).toString(), "app3");
}

/*
 * Test that an application launched by shell itself creates the correct Application instance and
 * emits signals indicating the model updated
 */
TEST_F(ApplicationManagerTests,appStartedByShell)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString name("Test App");

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));
    ON_CALL(*mockDesktopFileReader, name()).WillByDefault(Return(name));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy addedSpy(&applicationManager, SIGNAL(applicationAdded(const QString &)));

    // start the application
    Application *theApp = applicationManager.startApplication(appId, ApplicationManager::NoFlag);

    // check application data
    EXPECT_EQ(theApp->state(), Application::Starting);
    EXPECT_EQ(theApp->appId(), appId);
    EXPECT_EQ(theApp->name(), name);
    EXPECT_EQ(theApp->canBeResumed(), true);

    // check signals were emitted
    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(addedSpy.count(), 1);
    EXPECT_EQ(addedSpy.takeFirst().at(0).toString(), appId);

    // check application in list of apps
    Application *theAppAgain = applicationManager.findApplication(appId);
    EXPECT_EQ(theAppAgain, theApp);
}

/*
 * Test that an application launched upstart (i.e. not by shell itself) creates the correct Application
 * instance and emits signals indicating the model updated
 */
TEST_F(ApplicationManagerTests,appStartedByUpstart)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString name("Test App");

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));
    ON_CALL(*mockDesktopFileReader, name()).WillByDefault(Return(name));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy addedSpy(&applicationManager, SIGNAL(applicationAdded(const QString &)));
    QSignalSpy focusSpy(&applicationManager, SIGNAL(focusRequested(const QString &)));

    // upstart sends notification that the application was started
    applicationManager.onProcessStarting(appId);

    Application *theApp = applicationManager.findApplication(appId);

    // check application data
    EXPECT_EQ(theApp->state(), Application::Starting);
    EXPECT_EQ(theApp->appId(), appId);
    EXPECT_EQ(theApp->name(), name);
    EXPECT_EQ(theApp->canBeResumed(), true);

    // check signals were emitted
    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(addedSpy.count(), 1);
    EXPECT_EQ(addedSpy.takeFirst().at(0).toString(), appId);
    EXPECT_EQ(focusSpy.count(), 1);
    EXPECT_EQ(focusSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that an application launched via the command line with a correct --desktop_file_hint is accepted,
 * creates the correct Application instance and emits signals indicating the model updated
 */
TEST_F(ApplicationManagerTests,appStartedUsingCorrectDesktopFileHintSwitch)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString name("Test App");
    quint64 procId = 5551;
    QByteArray cmdLine("/usr/bin/testApp --desktop_file_hint=");
    cmdLine = cmdLine.append(appId);

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));
    ON_CALL(*mockDesktopFileReader, name()).WillByDefault(Return(name));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy addedSpy(&applicationManager, SIGNAL(applicationAdded(const QString &)));

    // Mir requests authentication for an application that was started
    bool authed = false;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, true);

    Application *theApp = applicationManager.findApplication(appId);

    // check application data
    EXPECT_EQ(theApp->state(), Application::Starting);
    EXPECT_EQ(theApp->appId(), appId);
    EXPECT_EQ(theApp->name(), name);
    EXPECT_EQ(theApp->canBeResumed(), false);

    // check signals were emitted
    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(addedSpy.count(), 1);
    EXPECT_EQ(addedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that an application launched via the command line without the correct --desktop_file_hint is rejected
 */
TEST_F(ApplicationManagerTests,appDoesNotStartWhenUsingBadDesktopFileHintSwitch)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString name("Test App");
    quint64 procId = 5551;
    QByteArray cmdLine("/usr/bin/testApp");

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy addedSpy(&applicationManager, SIGNAL(applicationAdded(const QString &)));

    // Mir requests authentication for an application that was started
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, false);

    Application *theApp = applicationManager.findApplication(appId);

    EXPECT_EQ(theApp, nullptr);

    // check no new signals were emitted
    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(addedSpy.count(), 0);
}

/*
 * Test that an application launched via the command line with the --desktop_file_hint but an incorrect
 * desktop file specified is rejected
 */
TEST_F(ApplicationManagerTests,appDoesNotStartWhenUsingBadDesktopFileHintFile)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString badDesktopFile = QString("%1.desktop").arg(appId);
    quint64 procId = 5551;
    QByteArray cmdLine("/usr/bin/testApp --desktop_file_hint=");
    cmdLine = cmdLine.append(badDesktopFile);

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(false));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    // Mir requests authentication for an application that was started, should fail
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, false);
}

/*
 * Test that the child sessions of a webapp process are accepted
 */
TEST_F(ApplicationManagerTests,webAppSecondarySessionsAccepted)
{
    using namespace ::testing;
    quint64 procId = 5551;
    QByteArray cmdLine("/usr/bin/qt5/libexec/QtWebProcess");

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    bool authed = false;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, true);
}

/*
 * Test that maliit sessions are accepted
 */
TEST_F(ApplicationManagerTests,maliitSessionsAccepted)
{
    using namespace ::testing;
    quint64 procId = 151;
    QByteArray cmdLine("maliit-server --blah");

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    bool authed = false;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, true);
}

/*
 * Test that an application in the Starting state is not impacted by the upstart "Starting" message
 * for that application (i.e. the upstart message is effectively useless)
 */
TEST_F(ApplicationManagerTests,onceAppAddedToApplicationLists_upstartStartingEventIgnored)
{
    using namespace ::testing;
    const QString appId("testAppId");

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy addedSpy(&applicationManager, SIGNAL(applicationAdded(const QString &)));

    // upstart sends notification that the application was started
    applicationManager.onProcessStarting(appId);

    // check no new signals were emitted and application state unchanged
    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(addedSpy.count(), 0);

    Application *theApp = applicationManager.findApplication(appId);
    EXPECT_EQ(Application::Starting, theApp->state());
}

/*
 * Test that an application in the Starting state reacts correctly to the Mir sessionStarted
 * event for that application (i.e. the Session is associated)
 */
TEST_F(ApplicationManagerTests,onceAppAddedToApplicationLists_mirSessionStartingEventHandled)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy addedSpy(&applicationManager, SIGNAL(applicationAdded(const QString &)));

    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);

    // Authorize session and emit Mir sessionStarting event
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(addedSpy.count(), 0);

    // Check application state and session are correctly set
    Application *theApp = applicationManager.findApplication(appId);
    EXPECT_EQ(theApp->session()->session(), session);
    EXPECT_EQ(theApp->focused(), false);
}

/*
 * Test that an application in the Starting state reacts correctly to the Mir surfaceCreated
 * event for that application (i.e. the Surface is associated and state set to Running)
 */
TEST_F(ApplicationManagerTests,onceAppAddedToApplicationLists_mirSurfaceCreatedEventHandled)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);

    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);

    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);

    applicationManager.onSessionCreatedSurface(session.get(), surface);

    // Check application state is correctly set
    Application *theApp = applicationManager.findApplication(appId);
    EXPECT_EQ(theApp->state(), Application::Running);
}

/*
 * Test that an application is stopped correctly, if it has not yet created a surface (still in Starting state)
 */
TEST_F(ApplicationManagerTests,shellStopsAppCorrectlyBeforeSurfaceCreated)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Stop app
    applicationManager.stopApplication(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that the foreground application is stopped correctly (is in Running state, has surface)
 */
TEST_F(ApplicationManagerTests,shellStopsForegroundAppCorrectly)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    EXPECT_EQ(applicationManager.focusedApplicationId(), appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Stop app
    applicationManager.stopApplication(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that the background application is stopped correctly
 */
TEST_F(ApplicationManagerTests,shellStopsBackgroundAppCorrectly)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.unfocusCurrentApplication();

    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Stop app
    applicationManager.stopApplication(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if an application is stopped by upstart, before it has created a surface, AppMan cleans up after it ok
 */
TEST_F(ApplicationManagerTests,upstartNotificationOfStartingAppBeingStopped)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Upstart notifies of stopping app
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if the foreground Running application is stopped by upstart, AppMan cleans up after it ok
 */
TEST_F(ApplicationManagerTests,upstartNotifiesOfStoppingForegroundApp)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    EXPECT_EQ(applicationManager.focusedApplicationId(), appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Upstart notifies of stopping app
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if the foreground Running application is reported to unexpectedly stop by upstart, AppMan
 * cleans up after it ok (as was not in background, had not lifecycle saved its state, so cannot be resumed)
 */
TEST_F(ApplicationManagerTests,upstartNotifiesOfUnexpectedStopOfForegroundApp)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    EXPECT_EQ(applicationManager.focusedApplicationId(), appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Upstart notifies of crashing / OOM killed app
    applicationManager.onProcessFailed(appId, false);

    // Upstart finally notifies the app stopped
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if a background application is stopped by upstart, AppMan removes it from the app list
 * as the event is a result of direct user interaction
 */
TEST_F(ApplicationManagerTests,upstartNotifiesOfStoppingBackgroundApp)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.unfocusCurrentApplication();
    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy focusSpy(&applicationManager, SIGNAL(focusedApplicationIdChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Upstart notifies of stopping app
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(focusSpy.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if a background application is reported to unexpectedly stop by upstart, AppMan does not remove
 * it from the app lists but instead considers it Stopped, ready to be resumed. This is due to the fact the
 * app should have saved its state, so can be resumed. This situation can occur due to the OOM killer, or
 * a dodgy app crashing.
 */
TEST_F(ApplicationManagerTests,unexpectedStopOfBackgroundApp)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    applicationManager.unfocusCurrentApplication();
    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy focusSpy(&applicationManager, SIGNAL(focusedApplicationIdChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir reports disconnection
    onSessionStopping(session);

    // Upstart notifies of crashing / OOM-killed app
    applicationManager.onProcessFailed(appId, false);

    EXPECT_EQ(focusSpy.count(), 0);

    // Upstart finally notifies the app stopped
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(removedSpy.count(), 0);
}

/*
 * Test that if a background application is reported to have stopped on startup by upstart, that it
 * is kept in the application model, as the app can be lifecycle resumed.
 *
 * Note that upstart reports this "stopped on startup" even for applications which are running.
 * This may be an upstart bug, or AppMan mis-understanding upstart's intentions. But need to check
 * we're doing the right thing in either case. CHECKME(greyback)!
 */
TEST_F(ApplicationManagerTests,unexpectedStopOfBackgroundAppCheckingUpstartBug)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    applicationManager.unfocusCurrentApplication();
    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy focusSpy(&applicationManager, SIGNAL(focusedApplicationIdChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir reports disconnection
    onSessionStopping(session);

    // Upstart notifies of crashing app
    applicationManager.onProcessFailed(appId, true);

    EXPECT_EQ(focusSpy.count(), 0);

    // Upstart finally notifies the app stopped
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(removedSpy.count(), 0);
}

/*
 * Test that if a Starting application is then reported to be stopping by Mir, AppMan cleans up after it ok
 */
TEST_F(ApplicationManagerTests,mirNotifiesStartingAppIsNowStopping)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if a Running foreground application is reported to be stopping by Mir, AppMan cleans up after it ok
 */
TEST_F(ApplicationManagerTests,mirNotifiesOfStoppingForegroundApp)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    // Associate a surface so AppMan considers app Running, check focused
    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    EXPECT_EQ(applicationManager.focusedApplicationId(), appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Test that if a foreground application (one launched via desktop_file_hint) is reported to be stopping by
 * Mir, AppMan removes it from the model immediately
 */
TEST_F(ApplicationManagerTests,mirNotifiesOfStoppingForegroundAppLaunchedWithDesktopFileHint)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString name("Test App");
    quint64 procId = 5551;
    QByteArray cmdLine("/usr/bin/testApp --desktop_file_hint=");
    cmdLine = cmdLine.append(appId);

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));
    ON_CALL(*mockDesktopFileReader, name()).WillByDefault(Return(name));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    // Mir requests authentication for an application that was started
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, true);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    onSessionStarting(session);

    // Associate a surface so AppMan considers app Running, check focused
    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    EXPECT_EQ(applicationManager.focusedApplicationId(), appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);

    Application *app = applicationManager.findApplication(appId);
    EXPECT_EQ(nullptr, app);
}

/*
 * Test that if a background application is reported to be stopping by Mir, AppMan sets its state to Stopped
 * but does not remove it from the model
 */
TEST_F(ApplicationManagerTests,mirNotifiesOfStoppingBackgroundApp)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    // Associate a surface so AppMan considers app Running, check in background
    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    applicationManager.unfocusCurrentApplication();
    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(applicationManager.count(), 1);
    EXPECT_EQ(removedSpy.count(), 0);

    Application * app = applicationManager.findApplication(appId);
    EXPECT_NE(nullptr,app);
    EXPECT_EQ(app->state(), Application::Stopped);
}

/*
 * Test that if a background application (one launched via desktop_file_hint) is reported to be stopping by
 * Mir, AppMan removes it from the model immediately
 */
TEST_F(ApplicationManagerTests,mirNotifiesOfStoppingBackgroundAppLaunchedWithDesktopFileHint)
{
    using namespace ::testing;
    const QString appId("testAppId");
    const QString name("Test App");
    quint64 procId = 5551;
    QByteArray cmdLine("/usr/bin/testApp --desktop_file_hint=");
    cmdLine = cmdLine.append(appId);

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId))
        .Times(1)
        .WillOnce(Return(cmdLine));

    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));
    ON_CALL(*mockDesktopFileReader, name()).WillByDefault(Return(name));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    // Mir requests authentication for an application that was started
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    EXPECT_EQ(authed, true);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    onSessionStarting(session);

    // Associate a surface so AppMan considers app Running, check in background
    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session.get(), surface);
    applicationManager.focusApplication(appId);
    applicationManager.unfocusCurrentApplication();
    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);

    Application * app = applicationManager.findApplication(appId);
    EXPECT_EQ(nullptr,app);
}

/*
 * Test that when an application is stopped correctly by shell, the upstart stopping event is ignored
 */
TEST_F(ApplicationManagerTests,shellStoppedApp_upstartStoppingEventIgnored)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    applicationManager.stopApplication(appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Upstart notifies of stopping app
    applicationManager.onProcessStopped(appId);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(removedSpy.count(), 0);
}

/*
 * Test that when an application is stopped correctly by shell, the Mir Session stopped event is ignored
 */
TEST_F(ApplicationManagerTests,shellStoppedApp_mirSessionStoppingEventIgnored)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    applicationManager.stopApplication(appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app/Session
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(removedSpy.count(), 0);
}

/*
 * Test that if an application is stopped by upstart, the Mir stopping event is ignored
 */
TEST_F(ApplicationManagerTests,appStoppedByUpstart_mirSessionStoppingEventIgnored)
{
    using namespace ::testing;
    const QString appId("testAppId");
    quint64 procId = 5551;

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session = std::make_shared<MockSession>("", procId);
    bool authed = true;
    applicationManager.authorizeSession(procId, authed);
    onSessionStarting(session);

    applicationManager.onProcessStopped(appId);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app
    onSessionStopping(session);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(removedSpy.count(), 0);
}

/*
 * Webapps have multiple sessions, but only one is linked to the application (other is considered a hidden session).
 * If webapp in foreground stops unexpectedly, remove it and it alone from app list
 */
TEST_F(ApplicationManagerTests,unexpectedStopOfForegroundWebapp)
{
    using namespace ::testing;
    const QString appId("webapp");
    quint64 procId1 = 5551;
    quint64 procId2 = 5564;
    QByteArray cmdLine("/usr/bin/qt5/libexec/QtWebProcess");

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId2))
        .Times(1)
        .WillOnce(Return(cmdLine));

    ON_CALL(appController,appIdHasProcessId(procId1, appId)).WillByDefault(Return(true));
    ON_CALL(appController,appIdHasProcessId(procId2, _)).WillByDefault(Return(false));

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session1 = std::make_shared<MockSession>("", procId1);
    std::shared_ptr<mir::scene::Session> session2 = std::make_shared<MockSession>("", procId2);

    bool authed = false;
    applicationManager.authorizeSession(procId1, authed);
    onSessionStarting(session1);
    EXPECT_EQ(authed, true);
    applicationManager.authorizeSession(procId2, authed);
    onSessionStarting(session2);
    EXPECT_EQ(authed, true);
    std::shared_ptr<mir::scene::Surface> surface(nullptr);
    applicationManager.onSessionCreatedSurface(session2.get(), surface);

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app/Session
    onSessionStopping(session2);
    onSessionStopping(session1);

    EXPECT_EQ(countSpy.count(), 2); //FIXME(greyback)
    EXPECT_EQ(applicationManager.count(), 0);
    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), appId);
}

/*
 * Webapps have multiple sessions, but only one is linked to the application (other is considered a hidden session).
 * If webapp in background stops unexpectedly, do not remove it from app list
 */
TEST_F(ApplicationManagerTests,unexpectedStopOfBackgroundWebapp)
{
    using namespace ::testing;
    const QString appId("webapp");
    quint64 procId1 = 5551;
    quint64 procId2 = 5564;
    QByteArray cmdLine("/usr/bin/qt5/libexec/QtWebProcess");

    // Set up Mocks & signal watcher
    EXPECT_CALL(procInfo,command_line(procId2))
        .Times(1)
        .WillOnce(Return(cmdLine));

    ON_CALL(appController,appIdHasProcessId(procId1, appId)).WillByDefault(Return(true));
    ON_CALL(appController,appIdHasProcessId(procId2, _)).WillByDefault(Return(false));

    // Set up Mocks & signal watcher
    auto mockDesktopFileReader = new NiceMock<MockDesktopFileReader>(appId, QFileInfo());
    ON_CALL(*mockDesktopFileReader, loaded()).WillByDefault(Return(true));
    ON_CALL(*mockDesktopFileReader, appId()).WillByDefault(Return(appId));

    ON_CALL(desktopFileReaderFactory, createInstance(appId, _)).WillByDefault(Return(mockDesktopFileReader));

    EXPECT_CALL(appController, startApplicationWithAppIdAndArgs(appId, _))
        .Times(1)
        .WillOnce(Return(true));

    applicationManager.startApplication(appId, ApplicationManager::NoFlag);
    applicationManager.onProcessStarting(appId);
    std::shared_ptr<mir::scene::Session> session1 = std::make_shared<MockSession>("", procId1);
    std::shared_ptr<mir::scene::Session> session2 = std::make_shared<MockSession>("", procId2);

    bool authed = false;
    applicationManager.authorizeSession(procId1, authed);
    onSessionStarting(session1);
    EXPECT_EQ(authed, true);
    applicationManager.authorizeSession(procId2, authed);
    onSessionStarting(session2);
    EXPECT_EQ(authed, true);

    // both sessions create surfaces, then unfocus everything.
    std::shared_ptr<mir::scene::Surface> surface1(nullptr);
    applicationManager.onSessionCreatedSurface(session1.get(), surface1);
    std::shared_ptr<mir::scene::Surface> surface2(nullptr);
    applicationManager.onSessionCreatedSurface(session2.get(), surface2);
    applicationManager.focusApplication(appId);
    applicationManager.unfocusCurrentApplication();
    EXPECT_EQ(applicationManager.focusedApplicationId(), QString());

    QSignalSpy countSpy(&applicationManager, SIGNAL(countChanged()));
    QSignalSpy removedSpy(&applicationManager, SIGNAL(applicationRemoved(const QString &)));

    // Mir notifies of stopping app/Session
    onSessionStopping(session2);
    onSessionStopping(session1);

    EXPECT_EQ(countSpy.count(), 0);
    EXPECT_EQ(removedSpy.count(), 0);
}

/*
 * Test that screenshotting callback works cross thread.
 */
TEST_F(ApplicationManagerTests, threadedScreenshot)
{
    using namespace testing;
    quint64 procId1 = 5551;

    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;

    auto application = startApplication(procId1, "webapp");
    auto session = std::dynamic_pointer_cast<MockSession>(application->session()->session());
    ON_CALL(*session, take_snapshot(_)).WillByDefault(Invoke(
        [&](mir::scene::SnapshotCallback const& callback)
        {
            std::thread ([&, callback]() {
                std::unique_lock<std::mutex> lk(mutex);

                mir::scene::Snapshot snapshot{mir::geometry::Size{0,0},
                                              mir::geometry::Stride{0},
                                              nullptr};

                callback(snapshot);

                done = true;
                lk.unlock();
                cv.notify_one();
            }).detach();
        }));

    auto mockSurface = std::make_shared<MockSurface>();
    EXPECT_CALL(*session, default_surface()).WillRepeatedly(Return(mockSurface));

    {
        ApplicationScreenshotProvider screenshotProvider(&applicationManager);
        QSize actualSize;
        QSize requestedSize;
        QString imageId("webapp/123456");
        screenshotProvider.requestImage(imageId, &actualSize, requestedSize);
    }

    {
        std::unique_lock<decltype(mutex)> lk(mutex);
        cv.wait(lk, [&] { return done; } );
        EXPECT_TRUE(done);
    }

    applicationManager.stopApplication(application->appId());
}

/*
 * Test that screenshotting callback works when application has been deleted
 */
TEST_F(ApplicationManagerTests, threadedScreenshotAfterAppDelete)
{
    using namespace testing;
    quint64 procId1 = 5551;

    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;

    auto application = startApplication(procId1, "webapp");
    auto session = std::dynamic_pointer_cast<MockSession>(application->session()->session());
    ON_CALL(*session, take_snapshot(_)).WillByDefault(Invoke(
        [&](mir::scene::SnapshotCallback const& callback)
        {
            std::thread ([&, callback]() {
                mir::scene::Snapshot snapshot{mir::geometry::Size{0,0},
                                              mir::geometry::Stride{0},
                                              nullptr};

                // stop the application before calling the callback
                applicationManager.stopApplication(application->appId());

                callback(snapshot);

                done = true;
                cv.notify_one();
            }).detach();
        }));

    auto mockSurface = std::make_shared<MockSurface>();
    EXPECT_CALL(*session, default_surface()).WillRepeatedly(Return(mockSurface));

    {
        ApplicationScreenshotProvider screenshotProvider(&applicationManager);
        QSize actualSize;
        QSize requestedSize;
        QString imageId("webapp/123456");
        screenshotProvider.requestImage(imageId, &actualSize, requestedSize);
    }

    {
        std::unique_lock<decltype(mutex)> lk(mutex);
        cv.wait(lk, [&] { return done; } );
    }
}
