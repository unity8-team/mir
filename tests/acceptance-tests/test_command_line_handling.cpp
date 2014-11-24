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

#include "mir_test_framework/headless_test.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace testing;

struct CommandLineHandling : mir_test_framework::HeadlessTest
{
    MOCK_METHOD1(callback, void(std::vector<char const*> const&));

    std::function<void(int argc, char const* const* argv)> callback_functor =
        [this](int argc, char const* const* argv)
        {
            // Copy to a vector as ElementsAre() is convenient for checking
            std::vector<char const*> const copy{argv, argv+argc};
            callback(copy);
        };
};

TEST_F(CommandLineHandling, ValidOptionsAreAcceptedByDefaultCallback)
{
    char const* argv[] =
     { "dummy-exe-name", "--file", "test", "--enable-input", "off"};

    int const argc = std::distance(std::begin(argv), std::end(argv));

    server.set_command_line(argc, argv);

    server.apply_settings();
}

TEST_F(CommandLineHandling, UnrecognisedTokensCauseDefaultCallbackToThrow)
{
    char const* argv[] =
     { "dummy-exe-name", "--file", "test", "--hello", "world", "--enable-input", "off"};

    int const argc = std::distance(std::begin(argv), std::end(argv));

    server.set_command_line(argc, argv);

    EXPECT_THROW(server.apply_settings(), std::runtime_error);
}

TEST_F(CommandLineHandling, ValidOptionsAreNotPassedToCallback)
{
    char const* argv[] =
     { "dummy-exe-name", "--file", "test", "--enable-input", "off"};

    int const argc = std::distance(std::begin(argv), std::end(argv));

    server.set_command_line_handler(callback_functor);
    server.set_command_line(argc, argv);

    EXPECT_CALL(*this, callback(_)).Times(Exactly(0));

    server.apply_settings();
}

TEST_F(CommandLineHandling, UnrecognisedTokensArePassedToCallback)
{
    char const* argv[] =
     { "dummy-exe-name", "--file", "test", "--hello", "world", "--enable-input", "off"};

    int const argc = std::distance(std::begin(argv), std::end(argv));

    server.set_command_line_handler(callback_functor);
    server.set_command_line(argc, argv);

    EXPECT_CALL(*this, callback(ElementsAre(StrEq("--hello"), StrEq("world"))));

    server.apply_settings();
}
