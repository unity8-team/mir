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

#include "mir/graphics/android/mir_native_buffer.h"
#include <memory>
#include <gtest/gtest.h>

namespace mga=mir::graphics::android;

TEST(AndroidRefcount, driver_hooks)
{
    int call_count = 0;
    mga::MirNativeBuffer* driver_reference = nullptr;
    {
        auto tmp = new mga::MirNativeBuffer(
            [&](mga::MirNativeBuffer*)
            {
                call_count++;
            });
        mga::MirNativeBufferDeleter del;
        std::shared_ptr<mga::MirNativeBuffer> buffer(tmp, del);
        driver_reference = buffer.get();
        driver_reference->common.incRef(&driver_reference->common);
        //Mir loses its reference
    }

    EXPECT_EQ(0, call_count);
    driver_reference->common.decRef(&driver_reference->common);
    EXPECT_EQ(1, call_count);
}

TEST(AndroidRefcount, driver_hooks_mir_ref)
{
    int call_count = 0;
    {
        std::shared_ptr<mga::MirNativeBuffer> mir_reference;
        mga::MirNativeBuffer* driver_reference = nullptr;
        {
            auto tmp = new mga::MirNativeBuffer(
                [&](mga::MirNativeBuffer*)
                {
                    call_count++;
                });
            mga::MirNativeBufferDeleter del;
            mir_reference = std::shared_ptr<mga::MirNativeBuffer>(tmp, del);
            driver_reference = mir_reference.get();
            driver_reference->common.incRef(&driver_reference->common);
        }

        //driver loses its reference
        driver_reference->common.decRef(&driver_reference->common);
        EXPECT_EQ(0, call_count);
    }
    EXPECT_EQ(1, call_count);
}
