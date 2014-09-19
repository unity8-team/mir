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

#include <Unity/Application/session.h>

 #include "qtmir_test.h"

using namespace qtmir;
using mir::scene::MockSession;

namespace ms = mir::scene;

class SessionManagerTests : public ::testing::QtMirTest
{
public:
    SessionManagerTests()
    {}

    QList<std::shared_ptr<ms::PromptSession>> listPromptSessions(SessionInterface* session) {
        QList<std::shared_ptr<ms::PromptSession>> promptSessions;
        session->foreachPromptSession([&promptSessions](const std::shared_ptr<ms::PromptSession>& promptSession) {
            promptSessions << promptSession;
        });
        return promptSessions;
    }

    QList<SessionInterface*> listChildSessions(SessionInterface* session) {
        QList<SessionInterface*> sessions;
        session->foreachChildSession([&sessions](SessionInterface* session) {
            sessions << session;
        });
        return sessions;
    }
};

TEST_F(SessionManagerTests, sessionTracksPromptSession)
{
    using namespace testing;

    std::shared_ptr<ms::Session> mirAppSession = std::make_shared<MockSession>("mirAppSession", __LINE__);
    sessionManager.onSessionStarting(mirAppSession);
    SessionInterface* qtmirAppSession = sessionManager.findSession(mirAppSession.get());
    EXPECT_TRUE(qtmirAppSession != nullptr);

    auto promptSession = std::make_shared<ms::MockPromptSession>();
    ON_CALL(*mirConfig->the_mock_prompt_session_manager(), application_for(_)).WillByDefault(Return(mirAppSession));

    sessionManager.onPromptSessionStarting(promptSession);

    EXPECT_EQ(qtmirAppSession->activePromptSession(), promptSession);

    sessionManager.onPromptSessionStopping(promptSession);

    EXPECT_EQ(qtmirAppSession->activePromptSession(), nullptr);

    delete qtmirAppSession;
}


TEST_F(SessionManagerTests, TestPromptSession)
{
    using namespace testing;

    std::shared_ptr<ms::Session> mirAppSession = std::make_shared<MockSession>("mirAppSession", __LINE__);
    sessionManager.onSessionStarting(mirAppSession);
    SessionInterface* qtmirAppSession = sessionManager.findSession(mirAppSession.get());
    EXPECT_TRUE(qtmirAppSession != nullptr);

    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), application_for(_)).WillRepeatedly(Return(mirAppSession));
    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), helper_for(_)).WillRepeatedly(Return(nullptr));

    std::shared_ptr<ms::PromptSession> mirPromptSession = std::make_shared<ms::MockPromptSession>();

    // prompt provider session
    std::shared_ptr<ms::Session> mirProviderSession = std::make_shared<MockSession>("mirProviderSession", __LINE__);
    sessionManager.onSessionStarting(mirProviderSession);
    SessionInterface* qtmirProviderSession = sessionManager.findSession(mirProviderSession.get());

    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), for_each_provider_in(mirPromptSession,_)).WillRepeatedly(WithArgs<1>(Invoke(
        [&](std::function<void(std::shared_ptr<ms::Session> const& prompt_provider)> const& f) {
            f(mirProviderSession);
        })));

    EXPECT_THAT(listPromptSessions(qtmirAppSession), IsEmpty());

    sessionManager.onPromptSessionStarting(mirPromptSession);

    EXPECT_THAT(listPromptSessions(qtmirAppSession), ElementsAre(mirPromptSession));
    EXPECT_THAT(listChildSessions(qtmirAppSession), IsEmpty());

    sessionManager.onPromptProviderAdded(mirPromptSession.get(), mirProviderSession);

    EXPECT_THAT(listChildSessions(qtmirAppSession), ElementsAre(qtmirProviderSession));

    EXPECT_CALL(*mirConfig->the_mock_prompt_session_manager(), for_each_provider_in(mirPromptSession,_)).WillRepeatedly(InvokeWithoutArgs([]{}));

    EXPECT_EQ(qtmirProviderSession->live(), true);
    sessionManager.onPromptProviderRemoved(mirPromptSession.get(), mirProviderSession);
    EXPECT_EQ(qtmirProviderSession->live(), false);

    sessionManager.onPromptSessionStopping(mirPromptSession);

    EXPECT_THAT(listPromptSessions(qtmirAppSession), IsEmpty());

    delete qtmirProviderSession;
    delete qtmirAppSession;
}
