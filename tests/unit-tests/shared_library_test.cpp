/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/shared_library.h"

#include <gtest/gtest.h>

#include <boost/exception/diagnostic_information.hpp>
#include <stdexcept>

#include "mir_test_framework/executable_path.h"

namespace mtf = mir_test_framework;

namespace
{
class HasSubstring
{
public:
    HasSubstring(char const* substring) : substring(substring) {}
    HasSubstring(std::string const& substring) : substring{substring.c_str()} {}

    friend::testing::AssertionResult operator,(std::string const& target, HasSubstring const& match)
    {
      if (std::string::npos != target.find(match.substring))
        return ::testing::AssertionSuccess();
      else
        return ::testing::AssertionFailure() <<
            "The target:\n\"" << target << "\"\n"
            "Does not contain:\n\"" << match.substring << "\"";
    }

private:
    char const* const substring;

    HasSubstring(HasSubstring const&) = delete;
    HasSubstring& operator=(HasSubstring const&) = delete;
};

#define MIR_EXPECT_THAT(target, condition) EXPECT_TRUE((target, condition))

class SharedLibrary : public testing::Test
{
public:
    SharedLibrary()
        : nonexistent_library{"imma_totally_not_a_library"},
          existing_library{mtf::client_platform("dummy.so")},
          nonexistent_function{"yo_dawg"},
          existing_function{"create_client_platform"},
          existent_version{MIR_CLIENT_PLATFORM_VERSION},
          nonexistent_version{"GOATS_ON_THE_GREEN"}
    {
    }

    std::string const nonexistent_library;
    std::string const existing_library;
    std::string const nonexistent_function;
    std::string const existing_function;
    std::string const existent_version;
    std::string const nonexistent_version;
};

}

TEST_F(SharedLibrary, load_nonexistent_library_fails)
{
    EXPECT_THROW({ mir::SharedLibrary nonexistent(nonexistent_library); }, std::runtime_error);
}

TEST_F(SharedLibrary, load_nonexistent_library_fails_with_useful_info)
{
    try
    {
        mir::SharedLibrary nonexistent(nonexistent_library);
    }
    catch (std::exception const& error)
    {
        auto info = boost::diagnostic_information(error);

        MIR_EXPECT_THAT(info, HasSubstring("cannot open shared object")) << "What went wrong";
        MIR_EXPECT_THAT(info, HasSubstring(nonexistent_library)) << "Name of library";
    }
}

TEST_F(SharedLibrary, load_valid_library_works)
{
    mir::SharedLibrary existing(existing_library);
}

TEST_F(SharedLibrary, load_nonexistent_function_fails)
{
    mir::SharedLibrary existing(existing_library);

    EXPECT_THROW({ existing.load_function<void(*)()>(nonexistent_function); }, std::runtime_error);
}

TEST_F(SharedLibrary, load_nonexistent_function_fails_with_useful_info)
{
    mir::SharedLibrary existing(existing_library);

    try
    {
        existing.load_function<void(*)()>(nonexistent_function);
    }
    catch (std::exception const& error)
    {
        auto info = boost::diagnostic_information(error);

        MIR_EXPECT_THAT(info, HasSubstring("undefined symbol")) << "What went wrong";
        MIR_EXPECT_THAT(info, HasSubstring(existing_library)) << "Name of library";
        MIR_EXPECT_THAT(info, HasSubstring(nonexistent_function)) << "Name of function";
    }
}

TEST_F(SharedLibrary, load_valid_function_works)
{
    mir::SharedLibrary existing(existing_library);
    existing.load_function<void(*)()>(existing_function);
}

TEST_F(SharedLibrary, load_valid_versioned_function_works)
{
    mir::SharedLibrary existing{existing_library};
    existing.load_function<void(*)()>(existing_function, existent_version);
}

TEST_F(SharedLibrary, load_invalid_versioned_function_fails_with_appropriate_error)
{
    mir::SharedLibrary existing{existing_library};

    try
    {
        existing.load_function<void(*)()>(existing_function, nonexistent_version);
    }
    catch (std::exception const& error)
    {
        auto info = boost::diagnostic_information(error);

        MIR_EXPECT_THAT(info, HasSubstring("undefined symbol")) << "What went wrong";
        MIR_EXPECT_THAT(info, HasSubstring(nonexistent_version)) << "Version info";
        MIR_EXPECT_THAT(info, HasSubstring(existing_library)) << "Name of library";
        MIR_EXPECT_THAT(info, HasSubstring(existing_function)) << "Name of function";
    }
}
