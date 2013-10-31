/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir/shell/default_display_arbitrator.h"
#include "mir/shell/session_container.h"
#include "mir/graphics/display_configuration_policy.h"
#include "mir/graphics/display_changer.h"
#include "mir/shell/broadcasting_session_event_sink.h"

#include "mir_test_doubles/null_display_configuration.h"
#include "mir_test_doubles/stub_display_configuration.h"
#include "mir_test_doubles/mock_shell_session.h"
#include "mir_test_doubles/stub_shell_session.h"
#include "mir_test/fake_shared.h"
#include "mir_test/display_config_matchers.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace mf = mir::frontend;
namespace msh = mir::shell;
namespace mg = mir::graphics;

namespace
{

class MockDisplayChanger : public mg::DisplayChanger
{
public:
    ~MockDisplayChanger() noexcept {}
    MOCK_METHOD0(configuration, std::shared_ptr<mg::DisplayConfiguration>());
    MOCK_METHOD1(configure, void(std::shared_ptr<mg::DisplayConfiguration> const&));
};

class MockDisplayConfigurationPolicy : public mg::DisplayConfigurationPolicy
{
public:
    ~MockDisplayConfigurationPolicy() noexcept {}
    MOCK_METHOD1(apply_to, void(mg::DisplayConfiguration&));
};

class StubSessionContainer : public msh::SessionContainer
{
public:
    void insert_session(std::shared_ptr<msh::Session> const& session)
    {
        sessions.push_back(session);
    }

    void remove_session(std::shared_ptr<msh::Session> const&)
    {
    }

    void for_each(std::function<void(std::shared_ptr<msh::Session> const&)> f) const
    {
        for (auto const& session : sessions)
            f(session);
    }

private:
    std::vector<std::shared_ptr<msh::Session>> sessions;
};

struct DefaultDisplayArbitratorTest : public ::testing::Test
{
    DefaultDisplayArbitratorTest()
    {
        using namespace testing;

        ON_CALL(mock_display_changer, configuration())
            .WillByDefault(Return(mt::fake_shared(base_config)));

        arbitrator = std::make_shared<msh::DefaultDisplayArbitrator>(
                      mt::fake_shared(mock_display_changer),
                      mt::fake_shared(mock_conf_policy),
                      mt::fake_shared(stub_session_container),
                      mt::fake_shared(session_event_sink));
    }

    testing::NiceMock<MockDisplayChanger> mock_display_changer;
    testing::NiceMock<MockDisplayConfigurationPolicy> mock_conf_policy;
    StubSessionContainer stub_session_container;
    msh::BroadcastingSessionEventSink session_event_sink;
    mtd::StubDisplayConfig base_config;
    std::shared_ptr<msh::DefaultDisplayArbitrator> arbitrator;
};

}

TEST_F(DefaultDisplayArbitratorTest, returns_active_configuration_from_display)
{
    using namespace testing;
    mtd::NullDisplayConfiguration conf;

    EXPECT_CALL(mock_display_changer, configuration())
        .Times(1)
        .WillOnce(Return(mt::fake_shared(conf)));

    auto returned_conf = arbitrator->active_configuration();
    EXPECT_EQ(&conf, returned_conf.get());
}

TEST_F(DefaultDisplayArbitratorTest, pauses_system_when_applying_new_configuration_for_focused_session)
{
    using namespace testing;
    mtd::NullDisplayConfiguration conf;
    auto session = std::make_shared<mtd::StubShellSession>();

    InSequence s;
    EXPECT_CALL(mock_display_changer, configure(_));

    session_event_sink.handle_focus_change(session);
    arbitrator->configure(session,
        mt::fake_shared(conf));
}

TEST_F(DefaultDisplayArbitratorTest, doesnt_apply_config_for_unfocused_session)
{
    using namespace testing;
    mtd::NullDisplayConfiguration conf;

    EXPECT_CALL(mock_display_changer, configure(_)).Times(0);

    arbitrator->configure(std::make_shared<mtd::StubShellSession>(),
        mt::fake_shared(conf));
}

TEST_F(DefaultDisplayArbitratorTest, handles_hardware_change_properly_when_pausing_system)
{
    using namespace testing;
    mtd::NullDisplayConfiguration conf;

    InSequence s;
    EXPECT_CALL(mock_conf_policy, apply_to(Ref(conf)));
    EXPECT_CALL(mock_display_changer, configure(_));
 
    arbitrator->configure_for_hardware_change(mt::fake_shared(conf));
}

TEST_F(DefaultDisplayArbitratorTest, handles_hardware_change_properly_when_retaining_system_state)
{
    using namespace testing;
    mtd::NullDisplayConfiguration conf;

    InSequence s;
    EXPECT_CALL(mock_conf_policy, apply_to(Ref(conf)));
    EXPECT_CALL(mock_display_changer, configure(_));

    arbitrator->configure_for_hardware_change(mt::fake_shared(conf));
}

TEST_F(DefaultDisplayArbitratorTest, hardware_change_doesnt_apply_base_config_if_per_session_config_is_active)
{
    using namespace testing;

    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();

    stub_session_container.insert_session(session1);
    arbitrator->configure(session1, conf);

    session_event_sink.handle_focus_change(session1);

    Mock::VerifyAndClearExpectations(&mock_display_changer);

    InSequence s;
    EXPECT_CALL(mock_display_changer, configure(_)).Times(0);
    arbitrator->configure_for_hardware_change(conf);
}

TEST_F(DefaultDisplayArbitratorTest, notifies_all_sessions_on_hardware_config_change)
{
    using namespace testing;
    mtd::NullDisplayConfiguration conf;
    mtd::MockShellSession mock_session1;
    mtd::MockShellSession mock_session2;

    stub_session_container.insert_session(mt::fake_shared(mock_session1));
    stub_session_container.insert_session(mt::fake_shared(mock_session2));

    EXPECT_CALL(mock_session1, send_display_config(_));
    EXPECT_CALL(mock_session2, send_display_config(_));

    arbitrator->configure_for_hardware_change(mt::fake_shared(conf));
}

TEST_F(DefaultDisplayArbitratorTest, focusing_a_session_with_attached_config_applies_config)
{
    using namespace testing;
    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();

    stub_session_container.insert_session(session1);
    arbitrator->configure(session1, conf);

    InSequence s;
    EXPECT_CALL(mock_display_changer, configure(_));

    session_event_sink.handle_focus_change(session1);
}

TEST_F(DefaultDisplayArbitratorTest, focusing_a_session_without_attached_config_applies_base_config)
{
    using namespace testing;
    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();
    auto session2 = std::make_shared<mtd::StubShellSession>();

    stub_session_container.insert_session(session1);
    arbitrator->configure(session1, conf);

    session_event_sink.handle_focus_change(session1);

    Mock::VerifyAndClearExpectations(&mock_display_changer);
    // Restore expectation on baseconfig...change display changer to accept ref...
    EXPECT_CALL(mock_display_changer, configure(_)).Times(1);

    session_event_sink.handle_focus_change(session2);
}

TEST_F(DefaultDisplayArbitratorTest, losing_focus_applies_base_config)
{
    using namespace testing;
    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();

    stub_session_container.insert_session(session1);
    arbitrator->configure(session1, conf);

    session_event_sink.handle_focus_change(session1);

    Mock::VerifyAndClearExpectations(&mock_display_changer);

    // Restore matcher
    EXPECT_CALL(mock_display_changer, configure(_));

    session_event_sink.handle_no_focus();
}

TEST_F(DefaultDisplayArbitratorTest, base_config_is_not_applied_if_already_active)
{
    using namespace testing;
    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();
    auto session2 = std::make_shared<mtd::StubShellSession>();

    EXPECT_CALL(mock_display_changer, configure(_)).Times(0);

    stub_session_container.insert_session(session1);
    stub_session_container.insert_session(session2);

    session_event_sink.handle_focus_change(session1);
    session_event_sink.handle_focus_change(session2);
    session_event_sink.handle_no_focus();
}

TEST_F(DefaultDisplayArbitratorTest, hardware_change_invalidates_session_configs)
{
    using namespace testing;
    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();

    stub_session_container.insert_session(session1);
    arbitrator->configure(session1, conf);

    arbitrator->configure_for_hardware_change(conf);

    Mock::VerifyAndClearExpectations(&mock_display_changer);

    /*
     * Session1 had a config, but it should have been invalidated by the hardware
     * change, so expect no reconfiguration.
     */
    EXPECT_CALL(mock_display_changer, configure(_)).Times(0);

    session_event_sink.handle_focus_change(session1);
}

TEST_F(DefaultDisplayArbitratorTest, session_stopping_invalidates_session_config)
{
    using namespace testing;
    auto conf = std::make_shared<mtd::NullDisplayConfiguration>();
    auto session1 = std::make_shared<mtd::StubShellSession>();

    stub_session_container.insert_session(session1);
    arbitrator->configure(session1, conf);

    session_event_sink.handle_session_stopping(session1);

    Mock::VerifyAndClearExpectations(&mock_display_changer);

    /*
     * Session1 had a config, but it should have been invalidated by the
     * session stopping event, so expect no reconfiguration.
     */
    EXPECT_CALL(mock_display_changer, configure(_)).Times(0);

    session_event_sink.handle_focus_change(session1);
}
