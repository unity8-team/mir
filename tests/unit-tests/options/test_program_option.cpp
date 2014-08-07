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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/options/program_option.h"

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>

#include <cstdlib>

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace bpo = boost::program_options;

namespace
{
struct ProgramOption : testing::Test
{
    ProgramOption() :
        desc("Options")
    {
        desc.add_options()
            ("file,f", bpo::value<std::string>(), "<socket filename>")
            ("flag-yes", bpo::value<bool>(), "flag \"yes\"")
            ("flag-true", bpo::value<bool>(), "flag \"true\"")
            ("flag-no", bpo::value<bool>(), "flag \"no\"")
            ("flag-false", bpo::value<bool>(), "flag \"false\"")
            ("count,c", bpo::value<int>(), "count")
            ("defaulted,d", bpo::value<int>()->default_value(666), "defaulted")
            ("help,h", "this help text");
    }

    bpo::options_description desc;
};
}

TEST_F(ProgramOption, parse_device_line_long)
{
    mir::options::ProgramOption po;

    const int argc = 3;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "--file", "test_file"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_EQ("test_file", po.get("file", "default"));
    EXPECT_EQ("default", po.get("garbage", "default"));
    EXPECT_TRUE(po.is_set("file"));
    EXPECT_FALSE(po.is_set("garbage"));
}

TEST_F(ProgramOption, parse_device_line_short)
{
    mir::options::ProgramOption po;

    const int argc = 3;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "-f", "test_file"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_EQ("test_file", po.get("file", "default"));
    EXPECT_EQ("default", po.get("garbage", "default"));
    EXPECT_TRUE(po.is_set("file"));
    EXPECT_FALSE(po.is_set("garbage"));
}

TEST_F(ProgramOption, parse_device_yes_no)
{
    mir::options::ProgramOption po;

    const int argc = 9;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "--flag-yes", "yes",
        "--flag-true", "true",
        "--flag-no", "no",
        "--flag-false", "false"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_TRUE(po.get("flag-yes", false));
    EXPECT_TRUE(po.get("flag-true", false));
    EXPECT_FALSE(po.get("flag-no", true));
    EXPECT_FALSE(po.get("flag-false", true));

    EXPECT_FALSE(po.get("flag-default", false));
    EXPECT_TRUE(po.get("flag-default", true));
}

TEST_F(ProgramOption, parse_device_line_help)
{
    mir::options::ProgramOption po;

    const int argc = 2;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "--help"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_TRUE(po.is_set("help"));
}

TEST_F(ProgramOption, matches_compound_name_lookup)
{
    using testing::Eq;
    mir::options::ProgramOption po;

    const int argc = 8;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "--help",
        "--flag-yes", "yes",
        "-f", "test_file",
        "-c", "27"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_THAT(po.is_set("help,h"), Eq(true));
    EXPECT_THAT(po.get("flag-yes,y", false), Eq(true));
    EXPECT_THAT(po.get("file,f", "default"), Eq("test_file"));
    EXPECT_THAT(po.get("count,c", 42), Eq(27));
}

TEST_F(ProgramOption, defaulted_values_are_available)
{
    using testing::Eq;
    mir::options::ProgramOption po;

    const int argc = 1;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_THAT(po.is_set("defaulted"), Eq(true));
    EXPECT_THAT(po.get("defaulted", 3), Eq(666));
}

TEST_F(ProgramOption, test_boost_any_overload)
{
    using testing::Eq;
    mir::options::ProgramOption po;

    const int argc = 8;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "--help",
        "--flag-yes", "yes",
        "-f", "test_file",
        "-c", "27"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_THAT(po.is_set("help,h"), Eq(true));
    EXPECT_THAT(po.get<bool>("flag-yes,y"), Eq(true));
    EXPECT_THAT(po.get<std::string>("file,f"), Eq("test_file"));
    EXPECT_THAT(po.get<int>("count,c"), Eq(27));
    EXPECT_THAT(po.get<int>("defaulted"), Eq(666));

    auto const error_result = po.get("garbage");
    EXPECT_THAT(error_result.empty(), Eq(true));

    EXPECT_THROW(boost::any_cast<int>(error_result), std::bad_cast);
    EXPECT_THROW(po.get<int>("flag-yes,y"), std::bad_cast);
}

TEST_F(ProgramOption, unparsed_command_line_returns_unprocessed_tokens)
{
    using namespace testing;

    mir::options::ProgramOption po;

    const int argc = 11;
    char const* argv[argc] = {
        __PRETTY_FUNCTION__,
        "--flag-yes", "yes",
        "--hello",
        "-f", "test_file",
        "world",
        "-c", "27",
        "--answer", "42"
    };

    po.parse_arguments(desc, argc, argv);

    EXPECT_THAT(po.unparsed_command_line(), ElementsAre("--hello", "world", "--answer", "42"));
}

TEST(ProgramOptionEnv, parse_environment)
{
    // Env variables should be uppercase and "_" delimited
    char const* name = "some-key";
    char const* key = "SOME_KEY";
    char const* value = "test_value";
    auto const env = std::string(__PRETTY_FUNCTION__) + key;
    setenv(env.c_str(), value, true);

    bpo::options_description desc;
    desc.add_options()
        (name, bpo::value<std::string>());

    mir::options::ProgramOption po;
    po.parse_environment(desc, __PRETTY_FUNCTION__);

    EXPECT_EQ(value, po.get(name, "default"));
    EXPECT_EQ("default", po.get("garbage", "default"));
    EXPECT_TRUE(po.is_set(name));
    EXPECT_FALSE(po.is_set("garbage"));
}

// TODO need to parse something
TEST(ProgramOptionFile, parse_files)
{
    bpo::options_description desc("Config file options");

    mir::options::ProgramOption po;

    po.parse_file(desc, "test.config");
}
