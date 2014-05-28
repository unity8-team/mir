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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#ifndef MIR_MOCK_SERVER_ACTION_QUEUE_H_
#define MIR_MOCK_SERVER_ACTION_QUEUE_H_

#include "mir/server_action_queue.h"
#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

class MockServerActionQueue : public mir::ServerActionQueue
{
public:
    MOCK_METHOD2(enqueue, void (void const* /*owner*/, mir::ServerAction const& /*action*/));
    MOCK_METHOD1(pause_processing_for, void(void const* /*owner*/));
    MOCK_METHOD1(resume_processing_for, void(void const* /*owner*/));
};

}
}
}

#endif
