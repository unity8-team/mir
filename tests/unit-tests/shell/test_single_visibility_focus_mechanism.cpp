/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored By: Robert Carr <racarr@canonical.com>
 */

#include "mir/shell/application_session.h"
#include "mir/shell/default_session_container.h"
#include "mir/shell/registration_order_focus_sequence.h"
#include "mir/shell/single_visibility_focus_mechanism.h"
#include "mir/shell/session.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_surface_factory.h"
#include "mir_test_doubles/stub_surface.h"
#include "mir_test_doubles/mock_surface.h"
#include "mir_test_doubles/stub_surface_builder.h"
#include "mir_test_doubles/stub_input_targeter.h"
#include "mir_test_doubles/mock_input_targeter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace mc = mir::compositor;
namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace mf = mir::frontend;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

struct MockShellSession : public msh::Session
{
    MOCK_METHOD2(associate_surface, mf::SurfaceId(std::weak_ptr<ms::Surface> const&,
                                                  std::shared_ptr<mf::Surface> const&));
    MOCK_METHOD1(disassociate_surface, void(mf::SurfaceId surface));
    MOCK_CONST_METHOD1(get_surface, std::shared_ptr<mf::Surface>(mf::SurfaceId));

    MOCK_CONST_METHOD0(default_surface, std::shared_ptr<mf::Surface>());

    MOCK_CONST_METHOD0(name, std::string());
    MOCK_METHOD0(force_requests_to_complete, void());

    MOCK_METHOD0(hide, void());
    MOCK_METHOD0(show, void());

    MOCK_METHOD3(configure_surface, int(mf::SurfaceId, MirSurfaceAttrib, int));
};

TEST(SingleVisibilityFocusMechanism, mechanism_sets_visibility)
{
    using namespace ::testing;

    NiceMock<MockShellSession> app1, app2, app3;
    msh::DefaultSessionContainer model;

    ON_CALL(app1, default_surface()).WillByDefault(Return(std::shared_ptr<msh::Surface>()));
    ON_CALL(app2, default_surface()).WillByDefault(Return(std::shared_ptr<msh::Surface>()));
    ON_CALL(app3, default_surface()).WillByDefault(Return(std::shared_ptr<msh::Surface>()));

    msh::SingleVisibilityFocusMechanism focus_mechanism(mt::fake_shared(model),
                                                        std::make_shared<mtd::StubInputTargeter>());

    EXPECT_CALL(app1, show()).Times(1);
    EXPECT_CALL(app2, hide()).Times(1);
    EXPECT_CALL(app3, hide()).Times(1);

    EXPECT_CALL(app1, name()).Times(AnyNumber());
    EXPECT_CALL(app2, name()).Times(AnyNumber());
    EXPECT_CALL(app3, name()).Times(AnyNumber());

    model.insert_session(mt::fake_shared(app1));
    model.insert_session(mt::fake_shared(app2));
    model.insert_session(mt::fake_shared(app3));

    focus_mechanism.set_focus_to(mt::fake_shared(app1));
}

TEST(SingleVisibilityFocusMechanism, sets_input_focus)
{
    using namespace ::testing;
    
    NiceMock<MockShellSession> app1;
    mtd::MockSurface mock_surface;
    {
        InSequence seq;
        EXPECT_CALL(app1, default_surface()).Times(1)
            .WillOnce(Return(mt::fake_shared(mock_surface)));
        EXPECT_CALL(app1, default_surface()).Times(1)
            .WillOnce(Return(std::shared_ptr<msh::Surface>()));
    }

    msh::DefaultSessionContainer model;
    model.insert_session(mt::fake_shared(app1));
    mtd::MockInputTargeter targeter;
    
    msh::SingleVisibilityFocusMechanism focus_mechanism(mt::fake_shared(model),
        mt::fake_shared(targeter));
    
    {
        InSequence seq;
        EXPECT_CALL(mock_surface, take_input_focus(_)).Times(1);
        // When we have no default surface.
        EXPECT_CALL(targeter, focus_cleared()).Times(1);
        // When we have no session.
        EXPECT_CALL(targeter, focus_cleared()).Times(1);
    }
    
    focus_mechanism.set_focus_to(mt::fake_shared(app1));
    focus_mechanism.set_focus_to(mt::fake_shared(app1));
    focus_mechanism.set_focus_to(std::shared_ptr<msh::Session>());
}
