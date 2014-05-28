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

#ifndef MIR_MOCK_TIMER_SERVICE_H_
#define MIR_MOCK_TIMER_SERVICE_H_

#include "mir/time/timer_service.h"

#include "mir_test/gmock_fixes.h"

namespace mir
{
namespace test
{
namespace doubles
{

class MockTimerService : public mir::time::TimerService
{
public:
    MOCK_METHOD2(notify_in,std::unique_ptr<mir::time::Alarm>(std::chrono::milliseconds,
                                                             std::function<void()>));
    MOCK_METHOD2(notify_at,std::unique_ptr<mir::time::Alarm>(mir::time::Timestamp,
                                                             std::function<void()>));

    MOCK_METHOD0(run, void());
    MOCK_METHOD0(stop, void());
};

}
}
}

#endif
