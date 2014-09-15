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

#ifndef MOCK_QTMIR_SESSION_H
#define MOCK_QTMIR_SESSION_H

#include <session_interface.h>
#include <gmock/gmock.h>

namespace qtmir {

class MockSession : public SessionInterface {
public:
    MockSession() : SessionInterface(0) {}

    MOCK_METHOD0(release, void());

    MOCK_CONST_METHOD0(name, QString());
    MOCK_CONST_METHOD0(application, unity::shell::application::ApplicationInfoInterface*());
    MOCK_CONST_METHOD0(surface, MirSurfaceItem*());
    MOCK_CONST_METHOD0(parentSession, SessionInterface*());
    MOCK_CONST_METHOD0(state, State());
    MOCK_CONST_METHOD0(fullscreen, bool());
    MOCK_CONST_METHOD0(live, bool());

    MOCK_METHOD1(setApplication, void(unity::shell::application::ApplicationInfoInterface* item));
    MOCK_METHOD1(setSurface, void(MirSurfaceItem* surface));
    MOCK_METHOD1(setState, void(State state));

    MOCK_METHOD1(addChildSession, void(SessionInterface* session));
    MOCK_METHOD2(insertChildSession, void(uint index, SessionInterface* session));
    MOCK_METHOD1(removeChildSession, void(SessionInterface* session));
    MOCK_CONST_METHOD1(foreachChildSession, void(std::function<void(SessionInterface* session)> f));

    MOCK_CONST_METHOD0(session, std::shared_ptr<mir::scene::Session>());

    MOCK_CONST_METHOD0(activePromptSession, std::shared_ptr<mir::scene::PromptSession>());
    MOCK_CONST_METHOD1(foreachPromptSession, void(std::function<void(const std::shared_ptr<mir::scene::PromptSession>&)> f));

    MOCK_CONST_METHOD0(childSessions, SessionModel*());

protected:
    MOCK_METHOD1(setFullscreen, void(bool fullscreen));
    MOCK_METHOD1(setLive, void(const bool));
    MOCK_METHOD1(appendPromptSession, void(const std::shared_ptr<mir::scene::PromptSession>& session));
    MOCK_METHOD1(removePromptSession, void(const std::shared_ptr<mir::scene::PromptSession>& session));
};

} // namespace qtmir

#endif // MOCK_QTMIR_SESSION_H
