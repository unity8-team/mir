/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */
#ifndef LIFECYCLE_HELPERS_H_
#define LIFECYCLE_HELPERS_H_

#include <utils/Looper.h>
#include <utils/threads.h>
#include <utils/Log.h>

namespace ubuntu
{
namespace application
{
struct ProcessKiller : public android::Thread
{
    ProcessKiller(const android::sp<ubuntu::detail::ApplicationSession>& as,
                  const android::sp<android::IAMTaskController>& controller) : as(as),
                                                                               task_controller(controller)
    {
    }

    bool threadLoop()
    {
        android::Mutex::Autolock _l(state_lock);
        state_cond.waitRelative(state_lock, seconds_to_nanoseconds(3)); // wait for timer

        if (as->running_state == ubuntu::application::ui::process_suspended)
        {
            ALOGI("%s() Suspending process", __PRETTY_FUNCTION__);
            if (task_controller == NULL)
                kill(as->pid, SIGSTOP); // delegate timer elapsed
            else
                task_controller->suspend_task(as->remote_pid);
        }

        return false;
    }

    android::sp<ubuntu::detail::ApplicationSession> as;
    android::sp<android::IAMTaskController> task_controller;
    android::Mutex state_lock;
    android::Condition state_cond;
    android::sp<android::Looper> looper;
};
}
}

#endif // LIFECYCLE_HELPERS_H_
