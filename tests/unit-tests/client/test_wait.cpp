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
 * Authored by: Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "src/client/mir_wait_handle.h"
#include <gtest/gtest.h>

TEST(MirWait, returns_correct_result)
{
    const char *apples = "apples";
    const char *oranges = "oranges";
    const char *pears = "pears";

    MirWaitHandle w;

    w.result_received((void*)apples);
    EXPECT_STREQ((const char *)w.wait_for_result(), apples);

    w.result_received((void*)oranges);
    EXPECT_STREQ((const char *)w.wait_for_result(), oranges);

    w.result_received((void*)pears);
    EXPECT_STREQ((const char *)w.wait_for_result(), pears);
}
