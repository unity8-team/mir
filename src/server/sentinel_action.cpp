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

#include "sentinel_action.h"


mir::SentinelAction::SentinelAction(
    ServerActionQueue & queue,
    boost::optional<std::thread::id> queue_thread_id,
    ServerAction const& action) :
    done{false}
{
    if (queue_thread_id &&
        *queue_thread_id != std::this_thread::get_id())
    {
        queue.enqueue(this,
                      [this,action]()
                      {
                          action();
                          std::lock_guard<std::mutex> lock(done_mutex);
                          done = true;
                          done_condition.notify_one();
                      });
    }
    else
    {
        done = true;
        action();
    }
}

mir::SentinelAction::~SentinelAction()
{
    if (done)
        return;

    std::unique_lock<std::mutex> lock(done_mutex);
    while(!done) done_condition.wait(lock);
}

