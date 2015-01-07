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

#include "mir/report/legacy_input_report.h"
#include "mir_test_doubles/mock_logger.h"

#include <std/Log.h>

#include "mir_test/fake_shared.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace ml = mir::logging;
namespace mri = mir::report::legacy_input;

using testing::_;

namespace
{
struct InputReport : public testing::Test
{
    mir::test::doubles::MockLogger logger;

    InputReport()
    {
        mri::initialize(mir::test::fake_shared(logger));
    }
};

char const* const component = "android-input";
char const* const LOG_TAG = "Foo";
}

TEST_F(InputReport, debug_message)
{
    // default minimum log priority is "informational". "debug" is lower than that.
    EXPECT_CALL(logger, log(_, _, _)).Times(0);

    ALOG(LOG_DEBUG, NULL, "Test function is %s", __PRETTY_FUNCTION__);
}

TEST_F(InputReport, unknown_message)
{
    char const* const unknown = "Unknown message";

    // default minimum log priority is "informational". "unknown" is lower than that.
    // Actually, I don't think this is even a valid priority.
    EXPECT_CALL(logger, log(_, _, _)).Times(0);

    ALOG(LOG_UNKNOWN, NULL, unknown);
}

TEST_F(InputReport, verbose_message)
{
    char const* const verbose = "A very long story. (OK, I lied.)";

    // default minimum log priority is "informational". "verbose" is lower than that.
    EXPECT_CALL(logger, log(_, _, _)).Times(0);

    ALOG(LOG_VERBOSE, NULL, verbose);
}

TEST_F(InputReport, info_message)
{
    EXPECT_CALL(logger, log(
            ml::Severity::informational,
            "[Foo]Some informational message",
            component));

    ALOGI("Some informational message");
}

TEST_F(InputReport, warning_message)
{
    EXPECT_CALL(logger, log(
            ml::Severity::warning,
            "[Foo]Warning!!!",
            component));

    ALOGW("Warning!!!");
}

TEST_F(InputReport, error_message)
{
    EXPECT_CALL(logger, log(
            ml::Severity::error,
            "[Foo]An error occurred!",
            component));

    ALOGE("An error occurred!");
}
