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
#include "mir/dispatch/action_queue.h"
#include "mir/dispatch/multiplexing_dispatchable.h"
#include "mir/dispatch/simple_dispatch_thread.h"

#include "mir/main_loop.h"
#include "mir/thread_name.h"
#include "mir/terminate_with_current_exception.h"

#include <condition_variable>
#include <mutex>

namespace mi = mir::input;
namespace mia = mi::android;

mi::DefaultInputManager::DefaultInputManager(std::shared_ptr<dispatch::MultiplexingDispatchable> const& multiplexer,
                                             std::shared_ptr<droidinput::InputReaderInterface> const& reader,
                                             std::shared_ptr<droidinput::EventHubInterface> const& event_hub)
    : multiplexer{multiplexer}, legacy_dispatchable{std::make_shared<mia::InputReaderDispatchable>(event_hub, reader)}, queue{std::make_shared<mir::dispatch::ActionQueue>()}, state{State::stopped}
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
    if (state == State::running)
        return;
    state = State::running;

    multiplexer->add_watch(queue);
    multiplexer->add_watch(legacy_dispatchable);

    legacy_dispatchable->start();

    queue->enqueue([this]()
                   {
                        mir::set_thread_name("Mir/InputReader");
                        for (auto const& platform : platforms)
                        {
                            platform->start();
                            multiplexer->add_watch(platform->dispatchable());
                        }
                   });

    input_thread = std::make_unique<dispatch::SimpleDispatchThread>(
        multiplexer,
        [this]()
        {
            state = State::stopped;
            mir::terminate_with_current_exception();
        });
}

void mi::DefaultInputManager::stop()
{
    if (state == State::stopped)
        return;
    std::mutex m;
    std::unique_lock<std::mutex> lock(m);
    std::condition_variable cv;
    bool done = false;

    queue->enqueue([&]()
                   {
                       for (auto const platform : platforms)
                       {
                           multiplexer->remove_watch(platform->dispatchable());
                           platform->stop();
                       }

                       std::unique_lock<std::mutex> lock(m);
                       done = true;
                       cv.notify_one();
                   });
    cv.wait(lock, [&]{return done;});
    state = State::stopped;

    input_thread.reset();

    multiplexer->remove_watch(legacy_dispatchable);
    multiplexer->remove_watch(queue);
}
