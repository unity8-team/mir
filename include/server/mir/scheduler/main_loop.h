/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_SCHEDULER_MAIN_LOOP_H_
#define MIR_SCHEDULER_MAIN_LOOP_H_

#include "mir/scheduler/event_handler_register.h"
#include "mir/scheduler/server_action_queue.h"

namespace mir
{
namespace scheduler
{

class MainLoop : public EventHandlerRegister,
                 public ServerActionQueue
{
public:
    virtual void run() = 0;
    virtual void stop() = 0;
};

}
}

#endif /* MIR_SCHEDULER_MAIN_LOOP_H_ */
