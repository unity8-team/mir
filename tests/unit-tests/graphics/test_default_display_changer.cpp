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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "src/server/graphics/default_display_changer.h"

#include "mir_test_doubles/mock_display.h"
#include "mir_test_doubles/mock_compositor.h"
#include "mir_test_doubles/null_display_configuration.h"

#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace mt = mir::test;
namespace mtd = mt::doubles;

namespace
{
struct DefaultDisplayChangerTest : public ::testing::Test
{
    DefaultDisplayChangerTest()
        : mock_display(std::make_shared<mtd::MockDisplay>()),
          mock_compositor(std::make_shared<mtd::MockCompositor>())
    {
        ON_CALL(*mock_display, configuration())
            .WillByDefault(testing::Return(mt::fake_shared(config)));
    }
    std::shared_ptr<mtd::MockDisplay> const mock_display;
    std::shared_ptr<mtd::MockCompositor> const mock_compositor;
    mtd::NullDisplayConfiguration config;
};
}

TEST_F(DefaultDisplayChangerTest, uses_configuration_from_display)
{
    using namespace testing;
    
    mg::DefaultDisplayChanger changer(mock_display, mock_compositor);
    
    EXPECT_CALL(*mock_display, configuration()).Times(1);
    
    auto returned_conf = changer.configuration();
    EXPECT_EQ(&config, returned_conf.get());
}

TEST_F(DefaultDisplayChangerTest, pauses_compositor_when_configuring_display)
{
    using namespace testing;
    
    mg::DefaultDisplayChanger changer(mock_display, mock_compositor);
    
    EXPECT_CALL(*mock_compositor, stop());
    EXPECT_CALL(*mock_display, configure(Ref(config)));
    EXPECT_CALL(*mock_compositor, start());
    
    changer.configure(mt::fake_shared(config));
}
