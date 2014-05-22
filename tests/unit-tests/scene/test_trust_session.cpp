/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored By: Nick Dedekind <nick.dedekind@canonical.com>
 */

#include "src/server/scene/trust_session_impl.h"
#include "src/server/scene/trust_session_container.h"
#include "mir/scene/trust_session_creation_parameters.h"
#include "mir_test/fake_shared.h"
#include "mir_test_doubles/stub_scene_session.h"
#include "mir_test_doubles/mock_trust_session_listener.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

using namespace testing;

namespace
{
struct MockTrustSessionContainer : ms::TrustSessionContainer
{
    MOCK_METHOD3(insert_participant, bool(ms::TrustSession* trust_session, std::weak_ptr<ms::Session> const& session, TrustType trust_type));

    MockTrustSessionContainer()
    {
        ON_CALL(*this, insert_participant(_, _, _)).WillByDefault(
            Invoke([this](ms::TrustSession* trust_session, std::weak_ptr<ms::Session> const& session, TrustType trust_type)
            {
                return ms::TrustSessionContainer::insert_participant(trust_session, session, trust_type);
            }));
    }
};

struct TrustSession : public testing::Test
{
    mtd::StubSceneSession stub_helper;
    mtd::StubSceneSession stub_app1;
    mtd::StubSceneSession stub_app2;

    std::shared_ptr<ms::Session> const trusted_session1 = mt::fake_shared(stub_app1);
    std::shared_ptr<ms::Session> const trusted_session2 = mt::fake_shared(stub_app2);

    NiceMock<mtd::MockTrustSessionListener> trust_session_listener;
    NiceMock<MockTrustSessionContainer> container;

    ms::TrustSessionImpl trust_session{
        mt::fake_shared(stub_helper),
        ms::a_trust_session(),
        mt::fake_shared(trust_session_listener),
        mt::fake_shared(container)};

    void SetUp()
    {
        container.insert_trust_session(mt::fake_shared(trust_session));
        trust_session.start();
    }

    void TearDown()
    {
        trust_session.stop();
    }
};

MATCHER_P(WeakPtrEq, p, "")
{
    return !arg.owner_before(p)
        && !p.owner_before(arg);
}
}

TEST_F(TrustSession, notifies_trusted_session_beginning_and_ending)
{
    Sequence session1;
    Sequence session2;
    Sequence added;

    EXPECT_CALL(trust_session_listener, trusted_session_beginning(Ref(trust_session), trusted_session1)).Times(1).InSequence(session1, added);
    EXPECT_CALL(trust_session_listener, trusted_session_beginning(Ref(trust_session), trusted_session2)).Times(1).InSequence(session2, added);

    EXPECT_CALL(trust_session_listener, trusted_session_ending(Ref(trust_session), trusted_session1)).Times(1).InSequence(session1);
    EXPECT_CALL(trust_session_listener, trusted_session_ending(Ref(trust_session), trusted_session2)).Times(1).InSequence(session2);

    trust_session.add_trusted_participant(trusted_session1);
    trust_session.add_trusted_participant(trusted_session2);
}

TEST_F(TrustSession, inserts_trusted_child_apps_in_container)
{
    InSequence seq;
    EXPECT_CALL(container, insert_participant(&trust_session, WeakPtrEq(trusted_session1), ms::TrustSessionContainer::TrustedSession));
    EXPECT_CALL(container, insert_participant(&trust_session, WeakPtrEq(trusted_session2), ms::TrustSessionContainer::TrustedSession));

    trust_session.add_trusted_participant(trusted_session1);
    trust_session.add_trusted_participant(trusted_session2);
}
