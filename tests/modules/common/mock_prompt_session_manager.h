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

#ifndef MOCK_MIR_SCENE_PROMPT_SESSION_MANAGER_H_
#define MOCK_MIR_SCENE_PROMPT_SESSION_MANAGER_H_

#include "mir/scene/prompt_session_manager.h"
#include "mir/scene/prompt_session_creation_parameters.h"

#include <gmock/gmock.h>

namespace mir {
namespace scene {

class MockPromptSessionManager: public PromptSessionManager
{
public:
    MOCK_CONST_METHOD2(start_prompt_session_for, std::shared_ptr<PromptSession>(std::shared_ptr<mir::scene::Session> const&,
        mir::scene::PromptSessionCreationParameters const&));

    MOCK_CONST_METHOD1(stop_prompt_session, void(std::shared_ptr<PromptSession> const&));

    MOCK_CONST_METHOD1(suspend_prompt_session, void(std::shared_ptr<PromptSession> const&));

    MOCK_CONST_METHOD1(resume_prompt_session, void(std::shared_ptr<PromptSession> const&));

    MOCK_CONST_METHOD2(add_prompt_provider, void(std::shared_ptr<PromptSession> const&,
        std::shared_ptr<mir::scene::Session> const&));

    MOCK_CONST_METHOD1(add_expected_session, void(std::shared_ptr<Session> const&));

    MOCK_CONST_METHOD1(remove_session, void(std::shared_ptr<Session> const&));

    MOCK_CONST_METHOD1(application_for, std::shared_ptr<Session>(std::shared_ptr<PromptSession> const&));

    MOCK_CONST_METHOD1(helper_for, std::shared_ptr<Session>(std::shared_ptr<PromptSession> const&));

    MOCK_CONST_METHOD2(for_each_provider_in, void(std::shared_ptr<PromptSession> const&,
        std::function<void(std::shared_ptr<Session> const&)> const&));
};

} // namespace scene
} // namespace mir

#endif // MOCK_MIR_SCENE_PROMPT_SESSION_MANAGER_H_
