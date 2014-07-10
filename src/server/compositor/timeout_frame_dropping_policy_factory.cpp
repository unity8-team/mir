/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#include "mir/compositor/frame_dropping_policy.h"

#include "timeout_frame_dropping_policy_factory.h"

#include <mutex>
#include <chrono>
#include <boost/throw_exception.hpp>

namespace mc = mir::compositor;

namespace
{
class TimeoutFrameDroppingPolicy : public mc::FrameDroppingPolicy
{
public:
    TimeoutFrameDroppingPolicy(std::shared_ptr<mir::time::Timer> const& timer,
                               std::chrono::milliseconds timeout,
                               std::function<void(void)> drop_frame);

    void swap_now_blocking() override;
    void swap_unblocked() override;

private:
    std::chrono::milliseconds const timeout;
    std::unique_ptr<mir::time::Alarm> const alarm;
    std::mutex mutex;
    unsigned int pending_swaps;
};

TimeoutFrameDroppingPolicy::TimeoutFrameDroppingPolicy(std::shared_ptr<mir::time::Timer> const& timer,
                                                       std::chrono::milliseconds timeout,
                                                       std::function<void(void)> drop_frame)
    : timeout{timeout},
      alarm{timer->create_alarm([this, drop_frame]
        {
            bool call_drop{false};
            bool reschedule{false};

            {
                std::lock_guard<std::mutex> lock{mutex};
                call_drop = pending_swaps > 0;
                reschedule = call_drop && --pending_swaps > 0;
            }
            if (call_drop) drop_frame();
            if (reschedule) alarm->reschedule_in(this->timeout);
        })},
      pending_swaps{0}
{
}

void TimeoutFrameDroppingPolicy::swap_now_blocking()
{
    if ([&]{ std::lock_guard<std::mutex> lock{mutex}; return pending_swaps++ == 0; }())
        alarm->reschedule_in(timeout);
}

void TimeoutFrameDroppingPolicy::swap_unblocked()
{
    if (alarm->state() == mir::time::Alarm::cancelled) return;

    bool update_alarm{false};
    bool reschedule{false};
    {
        std::lock_guard<std::mutex> lock{mutex};
        update_alarm = pending_swaps > 0;
        reschedule = update_alarm && --pending_swaps > 0;
    }

    if (update_alarm)
    {
        if (reschedule) alarm->reschedule_in(timeout);
        else alarm->cancel();
    }
}
}

mc::TimeoutFrameDroppingPolicyFactory::TimeoutFrameDroppingPolicyFactory(std::shared_ptr<mir::time::Timer> const& timer,
                                                                         std::chrono::milliseconds timeout)
    : timer{timer},
      timeout{timeout}
{
}


std::unique_ptr<mc::FrameDroppingPolicy> mc::TimeoutFrameDroppingPolicyFactory::create_policy(std::function<void ()> drop_frame) const
{
    return std::unique_ptr<mc::FrameDroppingPolicy>{new TimeoutFrameDroppingPolicy{timer, timeout, drop_frame}};
}
