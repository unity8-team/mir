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
 * Authored by: Andreas Pokorny <andreas.pokorny@canonical.com>
 */

#include "default_input_manager.h"
#include "android/input_reader_dispatchable.h"

#include "mir/input/platform.h"
#include "mir/input/legacy_input_dispatchable.h"
#include "mir/dispatch/action_queue.h"
#include "mir/dispatch/multiplexing_dispatchable.h"
#include "mir/dispatch/threaded_dispatcher.h"

#include "mir/main_loop.h"
#include "mir/thread_name.h"
#include "mir/terminate_with_current_exception.h"

#include <future>

namespace mi = mir::input;

mi::DefaultInputManager::DefaultInputManager(std::shared_ptr<dispatch::MultiplexingDispatchable> const& multiplexer,
                                             std::shared_ptr<LegacyInputDispatchable> const& legacy_dispatchable)
    : multiplexer{multiplexer}, legacy_dispatchable{legacy_dispatchable},
      queue{std::make_shared<mir::dispatch::ActionQueue>()}, state{State::stopped}
{
}

mi::DefaultInputManager::~DefaultInputManager()
{
    stop();
}

void mi::DefaultInputManager::add_platform(std::shared_ptr<Platform> const& platform)
{
    if (state == State::running)
    {
        queue->enqueue([this, platform]()
                       {
                           platforms.push_back(platform);
                           platform->start();
                           multiplexer->add_watch(platform->dispatchable());
                       });
    }
    else
    {
        queue->enqueue([this, platform]()
                       {
                           platforms.push_back(platform);
                       });
    }

}

void mi::DefaultInputManager::start()
{
    std::unique_lock<std::mutex> start_guard(start_stop_exclusion);

    if (state == State::running)
        return;

    state = State::running;

    multiplexer->add_watch(queue);
    multiplexer->add_watch(legacy_dispatchable);
    legacy_dispatchable->start();

    auto const started_promise = std::make_shared<std::promise<void>>();
    auto const weak_started_promise = std::weak_ptr<std::promise<void>>(started_promise);
    auto started_future = started_promise->get_future();

    queue->enqueue([this,weak_started_promise]()
                   {
                        for (auto const& platform : platforms)
                        {
                            platform->start();
                            multiplexer->add_watch(platform->dispatchable());
                        }
                        // TODO: Udev monitoring is still not separated yet - an initial scan is necessary to open
                        // devices, this will be triggered through the first call to dispatch->InputReader->loopOnce.
                        legacy_dispatchable->dispatch(dispatch::FdEvent::readable);
                        auto const started_promise =
                            std::shared_ptr<std::promise<void>>(weak_started_promise);
                        started_promise->set_value();
                   });

    input_thread = std::make_unique<dispatch::ThreadedDispatcher>(
        "Mir/Input Reader",
        multiplexer,
        [this,weak_started_promise]()
        {
            unregister_dispatchables();
            stop_platforms();

            state = State::stopped;
            if (auto started_promise = weak_started_promise.lock())
                started_promise->set_exception(std::current_exception());
            mir::terminate_with_current_exception();
        });

    started_future.wait();
}

void mi::DefaultInputManager::stop()
{
    std::unique_lock<std::mutex> start_guard(start_stop_exclusion);

    if (state == State::stopped)
        return;

    state = State::stopped;

    auto const stop_promise = std::make_shared<std::promise<void>>();

    queue->enqueue([stop_promise,this]()
                   {
                       stop_platforms();

                       stop_promise->set_value();
                   });

    stop_promise->get_future().wait();
    input_thread.reset();

    unregister_dispatchables();
}

void mi::DefaultInputManager::stop_platforms()
{
    for (auto const& platform : platforms)
    {
        multiplexer->remove_watch(platform->dispatchable());
        platform->stop();
    }
}

void mi::DefaultInputManager::unregister_dispatchables()
{
    multiplexer->remove_watch(legacy_dispatchable);
    multiplexer->remove_watch(queue);
}
