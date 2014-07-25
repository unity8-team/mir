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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_MOCK_FENCE_H_
#define MIR_TEST_DOUBLES_MOCK_FENCE_H_

#include "mir/graphics/android/fence.h"

#include "mir_test/gmock_fd_fix.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockFence : public graphics::android::Fence
{
    MOCK_METHOD0(wait, void());
    MOCK_METHOD1(merge_with_, void(Fd&));
    MOCK_CONST_METHOD0(copy_native_handle, Fd());
    void merge_with(mir::Fd fd)
    {
        return merge_with_(fd);
    }
};

}
}
}

#endif /* MIR_TEST_DOUBLES_MOCK_FENCE_H_ */
