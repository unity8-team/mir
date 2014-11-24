/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#include "src/server/scene/prompt_session_manager_impl.h"

#include "src/server/scene/session_container.h"

#include "mir/scene/prompt_session.h"
#include "mir/scene/prompt_session_creation_parameters.h"

#include "mir_test_doubles/mock_prompt_session_listener.h"
#include "mir_test_doubles/stub_scene_session.h"
#include "mir_test/fake_shared.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace ms = mir::scene;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

using namespace ::testing;

namespace
{
struct StubSessionContainer : ms::SessionContainer
{
    void insert_session(std::shared_ptr<ms::Session> const& session)
    {
        sessions.push_back(session);
    }

    void remove_session(std::shared_ptr<ms::Session> const&)
    {
    }

    void for_each(std::function<void(std::shared_ptr<ms::Session> const&)> f) const
    {
        for (auto const& session : sessions)
            f(session);
    }

    std::shared_ptr<ms::Session> successor_of(std::shared_ptr<ms::Session> const&) const
    {
        return {};
    }

    std::vector<std::shared_ptr<ms::Session>> sessions;
};

struct PromptSessionManager : public testing::Test
{
    pid_t const helper_pid = __LINE__;
    pid_t const application_pid = __LINE__;
    pid_t const prompt_provider_pid = __LINE__;
    std::shared_ptr<ms::Session> const helper{std::make_shared<mtd::StubSceneSession>(helper_pid)};
    std::shared_ptr<ms::Session> const application_session{std::make_shared<mtd::StubSceneSession>(application_pid)};
    std::shared_ptr<ms::Session> const provider_session{std::make_shared<mtd::StubSceneSession>(prompt_provider_pid)};
    std::shared_ptr<ms::Session> const another_prompt_provider{std::make_shared<mtd::StubSceneSession>(__LINE__)};
    ms::PromptSessionCreationParameters parameters;
    StubSessionContainer existing_sessions;

    NiceMock<mtd::MockPromptSessionListener> prompt_session_listener;

    ms::PromptSessionManagerImpl session_manager{mt::fake_shared(existing_sessions), mt::fake_shared(prompt_session_listener)};

    std::shared_ptr<ms::PromptSession> prompt_session;

    void SetUp()
    {
        existing_sessions.insert_session(application_session);

        parameters.application_pid = application_pid;
        prompt_session = session_manager.start_prompt_session_for(helper, parameters);
    }

    std::vector<std::shared_ptr<ms::Session>> list_providers_for(std::shared_ptr<ms::PromptSession> const& prompt_session)
    {
        std::vector<std::shared_ptr<ms::Session>> results;
        auto providers_fn = [&results](std::weak_ptr<ms::Session> const& session)
        {
            results.push_back(session.lock());
        };

        session_manager.for_each_provider_in(prompt_session, providers_fn);

        return results;
    }

    void helper_for()
    {
        session_manager.helper_for(prompt_session);
    }

    void application_for()
    {
        session_manager.application_for(prompt_session);
    }

    void iterate_providers()
    {
        session_manager.for_each_provider_in(prompt_session,
                                             [](std::shared_ptr<ms::Session> const&) {});
    }
};
}

TEST_F(PromptSessionManager, NotifiesProviderOfStartAndStop)
{
    InSequence seq;
    EXPECT_CALL(prompt_session_listener, starting(_)).Times(1);

    auto const prompt_session = session_manager.start_prompt_session_for(helper, parameters);

    EXPECT_CALL(prompt_session_listener, stopping(Eq(prompt_session))).Times(1);
    session_manager.stop_prompt_session(prompt_session);

    // Need to verify explicitly as we see unmatched callbacks during teardown of fixture
    Mock::VerifyAndClearExpectations(&prompt_session_listener);
}

TEST_F(PromptSessionManager, SetsHelperFor)
{
    EXPECT_EQ(session_manager.helper_for(prompt_session), helper);
}

TEST_F(PromptSessionManager, SetsApplicationFor)
{
    EXPECT_EQ(session_manager.application_for(prompt_session), application_session);
}

TEST_F(PromptSessionManager, SuccessfullyAddsAProvider)
{
    session_manager.add_prompt_provider(prompt_session, provider_session);

    EXPECT_THAT(list_providers_for(prompt_session), ElementsAre(provider_session));
}

TEST_F(PromptSessionManager, NoExceptionWhenAddingAPromptProviderTwice)
{
    session_manager.add_prompt_provider(prompt_session, provider_session);
    EXPECT_NO_THROW(session_manager.add_prompt_provider(prompt_session, provider_session));
}

TEST_F(PromptSessionManager, ThowsExceptionWhenAddingAPromptProviderWithStoppedPromptSession)
{
    session_manager.stop_prompt_session(prompt_session);

    EXPECT_THROW(
        session_manager.add_prompt_provider(prompt_session, provider_session),
        std::runtime_error);
}

TEST_F(PromptSessionManager, CanIterateOverPromptProvidersInAPromptSession)
{
    session_manager.add_prompt_provider(prompt_session, provider_session);
    session_manager.add_prompt_provider(prompt_session, another_prompt_provider);

    struct { MOCK_METHOD1(enumerate, void(std::shared_ptr<ms::Session> const& prompt_provider)); } mock;

    EXPECT_CALL(mock, enumerate(provider_session));
    EXPECT_CALL(mock, enumerate(another_prompt_provider));

    session_manager.for_each_provider_in(
        prompt_session,
        [&](std::shared_ptr<ms::Session> const& prompt_provider)
            { mock.enumerate(prompt_provider); });
}

TEST_F(PromptSessionManager, CanFetchApplicationDuringListenerNotifications)
{
    ON_CALL(prompt_session_listener, starting(_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::application_for));
    ON_CALL(prompt_session_listener, stopping(_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::application_for));
    ON_CALL(prompt_session_listener, prompt_provider_added(_,_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::application_for));
    ON_CALL(prompt_session_listener, prompt_provider_removed(_,_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::application_for));

    auto const prompt_session = session_manager.start_prompt_session_for(helper, parameters);
    session_manager.add_prompt_provider(prompt_session, provider_session);
    session_manager.remove_session(provider_session);
    session_manager.stop_prompt_session(prompt_session);
}

TEST_F(PromptSessionManager, CanFetchHelperDuringListenerNotifications)
{
    ON_CALL(prompt_session_listener, starting(_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::helper_for));
    ON_CALL(prompt_session_listener, stopping(_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::helper_for));
    ON_CALL(prompt_session_listener, prompt_provider_added(_,_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::helper_for));
    ON_CALL(prompt_session_listener, prompt_provider_removed(_,_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::helper_for));

    auto const prompt_session = session_manager.start_prompt_session_for(helper, parameters);
    session_manager.add_prompt_provider(prompt_session, provider_session);
    session_manager.remove_session(provider_session);
    session_manager.stop_prompt_session(prompt_session);
}

TEST_F(PromptSessionManager, CanIteratingOverPromptProvidersDuringListenerNotifications)
{
    ON_CALL(prompt_session_listener, starting(_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::iterate_providers));
    ON_CALL(prompt_session_listener, stopping(_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::iterate_providers));
    ON_CALL(prompt_session_listener, prompt_provider_added(_,_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::iterate_providers));
    ON_CALL(prompt_session_listener, prompt_provider_removed(_,_)).WillByDefault(InvokeWithoutArgs(this, &PromptSessionManager::iterate_providers));

    auto const prompt_session = session_manager.start_prompt_session_for(helper, parameters);
    session_manager.add_prompt_provider(prompt_session, provider_session);
    session_manager.remove_session(provider_session);
    session_manager.stop_prompt_session(prompt_session);
}
