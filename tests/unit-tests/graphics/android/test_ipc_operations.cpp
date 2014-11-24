/*
 * Copyright © 2014 Canonical Ltd.
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

#include "src/platform/graphics/android/ipc_operations.h"
#include "mir/graphics/platform_ipc_package.h"
#include <gtest/gtest.h>

namespace mg = mir::graphics;
namespace mga = mir::graphics::android;

struct IpcOperations : public ::testing::Test
{
    mga::IpcOperations ipc_operations;
};

/* ipc packaging tests */
TEST_F(IpcOperations, TestIpcDataPackedCorrectlyForFullIpc)
{
    //android has no valid operations platform specific operations yet, expect throw
    mg::PlatformIPCPackage package{
        std::vector<int32_t>{2,4,8,16,32},
        std::vector<int32_t>{fileno(tmpfile()), fileno(tmpfile())}
    };

    EXPECT_THROW({
        ipc_operations.platform_operation(0u, package);
    }, std::invalid_argument);

    EXPECT_THROW({
        ipc_operations.platform_operation(1u, package);
    }, std::invalid_argument);

    for (auto fd : package.ipc_fds)
        close(fd); 
}
