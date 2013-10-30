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

#include "mir/shell/unauthorized_display_arbitrator.h"

#include "mir_test_doubles/mock_display_arbitrator.h"
#include "mir_test_doubles/null_display_configuration.h"

#include "mir_test/fake_shared.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mt = mir::test;
namespace mtd = mir::test::doubles;
namespace mf = mir::frontend;
namespace msh = mir::shell;

struct UnauthorizedDisplayArbitratorTest : public ::testing::Test
{
    mtd::MockDisplayArbitrator underlying_arbitrator;
};

TEST_F(UnauthorizedDisplayArbitratorTest, change_attempt)
{
    mtd::NullDisplayConfiguration conf;
    msh::UnauthorizedDisplayArbitrator arbitrator(mt::fake_shared(underlying_arbitrator));

    EXPECT_THROW({
        arbitrator.configure(std::shared_ptr<mf::Session>(), mt::fake_shared(conf));
    }, std::runtime_error);
}

TEST_F(UnauthorizedDisplayArbitratorTest, access_config)
{
    using namespace testing;

    mtd::NullDisplayConfiguration conf;
    EXPECT_CALL(underlying_arbitrator, active_configuration())
        .Times(1)
        .WillOnce(Return(mt::fake_shared(conf)));

    msh::UnauthorizedDisplayArbitrator arbitrator(mt::fake_shared(underlying_arbitrator));

    auto returned_conf = arbitrator.active_configuration();

    EXPECT_EQ(&conf, returned_conf.get());
}
