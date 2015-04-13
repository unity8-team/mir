/*
 * Copyright © 2015 Canonical Ltd.
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

#ifndef MIR_TEST_DOUBLES_MOCK_HOST_STREAM_H_
#define MIR_TEST_DOUBLES_MOCK_HOST_STREAM_H_

#include "src/server/graphics/nested/host_stream.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockHostStream : graphics::nested::HostStream
{
    MOCK_CONST_METHOD0(current_buffer, std::weak_ptr<graphics::Buffer>());
    MOCK_METHOD0(swap, void());
};

}
}
}
#endif /* MIR_TEST_DOUBLES_MOCK_HOST_STREAM_H_ */
