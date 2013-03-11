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

#include "mir/surfaces/buffer_bundle.h"
#include "mir/shell/session.h"
#include "mir/shell/session_container.h"
#include "mir/shell/surface_creation_parameters.h"
#include "mir/surfaces/surface.h"

#include "mir_test/fake_shared.h"
#include "mir_test_doubles/mock_session.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace msh = mir::shell;
namespace mt = mir::test;
namespace mtd = mt::doubles;

TEST(SessionContainer, for_each)
{
    using namespace ::testing;
    msh::SessionContainer container;

    mtd::MockSession session1;
    mtd::MockSession session2;
    
    container.insert_session(mt::fake_shared(session1));
    container.insert_session(mt::fake_shared(session2));

    struct local
    {
        void operator()(std::shared_ptr<msh::Session> const& session)
        {
            session->name();
        }
    } functor;

    EXPECT_CALL(session1, name()).WillOnce(Return("VS 7"));
    EXPECT_CALL(session2, name()).WillOnce(Return("VS 8"));

    container.for_each(std::ref(functor));
}

TEST(SessionContainer, invalid_session_throw_behavior)
{
    using namespace ::testing;
    msh::SessionContainer container;
    mtd::MockSession invalid_session;

    EXPECT_THROW({
        container.remove_session(mt::fake_shared(invalid_session));
    }, std::logic_error);
}
