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

#ifndef MIR_SENTINEL_ACTION_H
#define MIR_SENTINEL_ACTION_H

#include "mir/server_action_queue.h"

#include <boost/optional.hpp>

#include <thread>
#include <mutex>
#include <condition_variable>

namespace mir
{

class SentinelAction
{
public:
    SentinelAction(ServerActionQueue & queue,
                   boost::optional<std::thread::id> queue_thread_id,
                   ServerAction const& action);
    ~SentinelAction();
private:
    std::mutex done_mutex;
    bool done;
    std::condition_variable done_condition;
};

}

#endif

