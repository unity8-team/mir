/*
 * Copyright (C) 2014 Canonical, Ltd.
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

#include <Unity/Application/application.h>
#include <Unity/Application/mirsurfaceitem.h>

#include "qtmir_test.h"
#include "stub_scene_surface.h"

using namespace qtmir;

namespace ms = mir::scene;
namespace mtd = mir::test::doubles;

class SessionTests : public ::testing::QtMirTest
{
public:
    SessionTests()
    {}

    QList<Session*> listChildSessions(Session* session) {
        QList<Session*> sessions;
        session->foreachChildSession([&sessions](Session* session) {
            sessions << session;
        });
        return sessions;
    }
};

TEST_F(SessionTests, AddChildSession)
{
    using namespace testing;

    const QString appId("test-app");
    quint64 procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, mirConfig->the_prompt_session_manager());
    Session session1(mirSession, mirConfig->the_prompt_session_manager());
    Session session2(mirSession, mirConfig->the_prompt_session_manager());
    Session session3(mirSession, mirConfig->the_prompt_session_manager());

    // add surfaces
    session.addChildSession(&session1);
    EXPECT_EQ(session1.parentSession(), &session);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1));

    session.addChildSession(&session2);
    EXPECT_EQ(session2.parentSession(), &session);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1, &session2));

    session.addChildSession(&session3);
    EXPECT_EQ(session3.parentSession(), &session);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1, &session2, &session3));
}

TEST_F(SessionTests, InsertChildSession)
{
    using namespace testing;

    const QString appId("test-app");
    quint64 procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, mirConfig->the_prompt_session_manager());
    Session session1(mirSession, mirConfig->the_prompt_session_manager());
    Session session2(mirSession, mirConfig->the_prompt_session_manager());
    Session session3(mirSession, mirConfig->the_prompt_session_manager());

    // add surfaces
    session.insertChildSession(100, &session1); // test overflow
    EXPECT_EQ(session1.parentSession(), &session);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1));

    session.insertChildSession(0, &session2); // test insert before
    EXPECT_EQ(session2.parentSession(), &session);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session2, &session1));

    session.insertChildSession(1, &session3); // test before end
    EXPECT_EQ(session3.parentSession(), &session);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session2, &session3, &session1));
}

TEST_F(SessionTests, RemoveChildSession)
{
    using namespace testing;

    const QString appId("test-app");
    quint64 procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, mirConfig->the_prompt_session_manager());
    Session session1(mirSession, mirConfig->the_prompt_session_manager());
    Session session2(mirSession, mirConfig->the_prompt_session_manager());
    Session session3(mirSession, mirConfig->the_prompt_session_manager());

    // add surfaces
    session.addChildSession(&session1);
    session.addChildSession(&session2);
    session.addChildSession(&session3);

    // remove surfaces
    session.removeChildSession(&session2);
    EXPECT_EQ(session2.parentSession(), nullptr);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1, &session3));

    session.removeChildSession(&session3);
    EXPECT_EQ(session3.parentSession(), nullptr);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(&session1));

    session.removeChildSession(&session1);
    EXPECT_EQ(session1.parentSession(), nullptr);
    EXPECT_THAT(listChildSessions(&session), IsEmpty());
}

TEST_F(SessionTests, DeleteChildSessionRemovesFromApplication)
{
    using namespace testing;

    const QString appId("test-app");
    quint64 procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session session(mirSession, mirConfig->the_prompt_session_manager());
    Session* session1 = new Session(mirSession, mirConfig->the_prompt_session_manager());
    Session* session2 = new Session(mirSession, mirConfig->the_prompt_session_manager());
    Session* session3 = new Session(mirSession, mirConfig->the_prompt_session_manager());

    // add surfaces
    session.addChildSession(session1);
    session.addChildSession(session2);
    session.addChildSession(session3);

    // delete surfaces
    delete session2;
    EXPECT_EQ(session2->parentSession(), nullptr);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(session1, session3));

    // delete surfaces
    delete session3;
    EXPECT_EQ(session3->parentSession(), nullptr);
    EXPECT_THAT(listChildSessions(&session), ElementsAre(session1));

    // delete surfaces
    delete session1;
    EXPECT_EQ(session1->parentSession(), nullptr);
    EXPECT_THAT(listChildSessions(&session), IsEmpty());
}

TEST_F(SessionTests, DeleteSessionDeletesChildSessions)
{
    using namespace testing;

    const QString appId("test-app");
    quint64 procId = 5551;

    std::shared_ptr<ms::Session> mirSession = std::make_shared<MockSession>(appId.toStdString(), procId);

    Session* session = new Session(mirSession, mirConfig->the_prompt_session_manager());
    Session* session1 = new Session(mirSession, mirConfig->the_prompt_session_manager());
    Session* session2 = new Session(mirSession, mirConfig->the_prompt_session_manager());
    Session* session3 = new Session(mirSession, mirConfig->the_prompt_session_manager());

    // add surfaces
    session->addChildSession(session1);
    session->addChildSession(session2);
    session->addChildSession(session3);

    QList<QObject*> destroyed;
    QObject::connect(session1, &QObject::destroyed, [&](QObject*) { destroyed << session1; });
    QObject::connect(session2, &QObject::destroyed, [&](QObject*) { destroyed << session2; });
    QObject::connect(session3, &QObject::destroyed, [&](QObject*) { destroyed << session3; });

    delete session;
    EXPECT_THAT(destroyed, UnorderedElementsAre(session1, session2, session3));
}
