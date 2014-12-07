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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "src/platform/input/evdev/libinput_wrapper.h"

#include "mir_test_doubles/mock_libinput.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mie = mir::input::evdev;

namespace
{
struct LibInputWrapper : public ::testing::Test
{
    mir::test::doubles::MockLibInput mock_libinput;
};
}

TEST_F(LibInputWrapper, tracks_a_libinput_path_context)
{
    using namespace ::testing;
    auto* fake_address = reinterpret_cast<libinput*>(0xF4C3);
    EXPECT_CALL(mock_libinput, libinput_path_create_context(_,_))
        .WillRepeatedly(Return(fake_address));
    EXPECT_CALL(mock_libinput, libinput_unref(fake_address));

    mie::LibInputWrapper wrapper;
}
