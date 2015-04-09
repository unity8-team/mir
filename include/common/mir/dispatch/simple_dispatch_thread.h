/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 */

#ifndef MIR_DISPATCH_SIMPLE_DISPATCH_THREAD_H_
#define MIR_DISPATCH_SIMPLE_DISPATCH_THREAD_H_

#include <memory>
#include <thread>
#include <future>
#include "mir/fd.h"

namespace mir
{
namespace dispatch
{
class Dispatchable;


class SimpleDispatchThread
{
public:
    SimpleDispatchThread(std::shared_ptr<Dispatchable> const& dispatchee);
    ~SimpleDispatchThread() noexcept;

    void steal_thread_on_self_destruction(std::promise<std::thread>& thread_promise);

private:
    Fd shutdown_fd;
    std::thread eventloop;
    std::promise<std::thread>* steal_thread_promise;
};

}
}


#endif // MIR_DISPATCH_SIMPLE_DISPATCH_THREAD_H_
