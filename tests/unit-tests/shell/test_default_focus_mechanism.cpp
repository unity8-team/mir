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
#include "mir/shell/registration_order_focus_sequence.h"
#include "mir/shell/default_focus_mechanism.h"
#include "mir/shell/session.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface.h"
#include "mir/graphics/display_configuration.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_buffer_stream.h"
#include "mir_test_doubles/mock_surface_factory.h"
#include "mir_test_doubles/mock_shell_session.h"
#include "mir_test_doubles/stub_surface.h"
#include "mir_test_doubles/mock_surface.h"
#include "mir_test_doubles/stub_surface_builder.h"
#include "mir_test_doubles/stub_surface_controller.h"
#include "mir_test_doubles/stub_input_targeter.h"
#include "mir_test_doubles/mock_input_targeter.h"
#include "mir_test_doubles/stub_surface_controller.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace mc = mir::compositor;
namespace msh = mir::shell;
namespace ms = mir::surfaces;
namespace mf = mir::frontend;
namespace mt = mir::test;
namespace mtd = mir::test::doubles;

TEST(DefaultFocusMechanism, hands_focus_to_session)
{
    using namespace ::testing;
    
    mtd::MockShellSession app1;
    EXPECT_CALL(app1, receive_focus(_, _)).Times(1);
    
    msh::DefaultFocusMechanism focus_mechanism(std::make_shared<mtd::StubInputTargeter>(), std::make_shared<mtd::StubSurfaceController>());

    focus_mechanism.set_focus_to(mt::fake_shared(app1));
}

TEST(DefaultFocusMechanism, relinquishes_focus_from_last_session)
{
    using namespace ::testing;
    
    auto app1 = std::make_shared<mtd::MockShellSession>();
    auto app2 = std::make_shared<mtd::MockShellSession>();
    
    {
        InSequence seq;
        EXPECT_CALL(*app1, receive_focus(_, _)).Times(1);
        EXPECT_CALL(*app1, relinquish_focus()).Times(1);
        EXPECT_CALL(*app2, receive_focus(_, _)).Times(1);
    }
    
    msh::DefaultFocusMechanism focus_mechanism(std::make_shared<mtd::StubInputTargeter>(), std::make_shared<mtd::StubSurfaceController>());

    focus_mechanism.set_focus_to(app1);
    focus_mechanism.set_focus_to(app2);
}
