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

using namespace mir_toolkit;

TEST(MirWait, no_callback)
{
    MirWaitHandle w;

    w.result_received();
    EXPECT_NO_FATAL_FAILURE(w.wait_for_result());
}

namespace
{
    void *cb_owner = nullptr;
    void *cb_context = nullptr;
}

void callback(void *owner, void *context)
{
    cb_owner = owner;
    cb_context = context;
}

TEST(MirWait, callback_before_signal)
{
    MirWaitHandle w;
    int alpha, beta, gamma, delta;

    cb_owner = 0;
    cb_context = 0;
    w.register_callback_owner(&alpha);
    w.register_callback(callback, &beta);
    w.result_received();
    w.wait_for_result();
    EXPECT_EQ(cb_owner, &alpha);
    EXPECT_EQ(cb_context, &beta);

    w.result_received();
    w.wait_for_result();
    EXPECT_EQ(cb_owner, &alpha);
    EXPECT_EQ(cb_context, &beta);

    cb_owner = 0;
    cb_context = 0;
    w.register_callback_owner(&gamma);
    w.register_callback(callback, &delta);
    w.result_received();
    w.wait_for_result();
    EXPECT_EQ(cb_owner, &gamma);
    EXPECT_EQ(cb_context, &delta);

    w.register_callback(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE({
        w.result_received();
        w.wait_for_result();
    });
}

TEST(MirWait, callback_after_signal)
{
    MirWaitHandle w;
    int alpha, beta, gamma, delta;

    cb_owner = 0;
    cb_context = 0;
    w.result_received();
    w.register_callback_owner(&alpha);
    w.register_callback(callback, &beta);
    w.wait_for_result();
    EXPECT_EQ(cb_owner, &alpha);
    EXPECT_EQ(cb_context, &beta);

    w.result_received();
    w.wait_for_result();
    EXPECT_EQ(cb_owner, &alpha);
    EXPECT_EQ(cb_context, &beta);

    cb_owner = 0;
    cb_context = 0;

    w.result_received();
    w.register_callback_owner(&gamma);
    w.register_callback(callback, &delta);
    w.wait_for_result();
    EXPECT_EQ(cb_owner, &gamma);
    EXPECT_EQ(cb_context, &delta);

    w.register_callback(nullptr, nullptr);
    EXPECT_NO_FATAL_FAILURE({
        w.result_received();
        w.wait_for_result();
    });
}
