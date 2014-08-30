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
 * Authored by: Alberto Aguirre <alberto.aguirre@canonical.com>
 */

#ifndef MIR_COMPOSITOR_THREAD_H_
#define MIR_COMPOSITOR_THREAD_H_

#include "mir/compositor/compositor.h"

#include <mutex>
#include <memory>
#include <thread>
#include <condition_variable>

namespace mir
{
namespace compositor
{

class CompositorLoop
{
public:
    CompositorLoop() = default;
    virtual ~CompositorLoop() = default;

    virtual void run() = 0;
    virtual void stop() = 0;
    virtual void schedule_compositing(int num_frames) = 0;

private:
    CompositorLoop(CompositorLoop const&) = delete;
    CompositorLoop& operator=(CompositorLoop const&) = delete;
};

enum class CompositorThreadState
{
    stopping,
    pausing,
    paused,
    running
};

class CompositorThread
{
public:
    CompositorThread(std::unique_ptr<CompositorLoop> loop);
    virtual ~CompositorThread();

    virtual void run(std::unique_ptr<CompositorLoop> loop);
    virtual void pause();
    virtual void schedule_compositing(int num_frames);

    virtual bool is_running() const;

private:
    void stop();
    void thread_entry() noexcept;
    void pause(std::unique_lock<std::mutex>& lock);

    CompositorThread(CompositorThread&) = delete;
    CompositorThread(CompositorThread const&) = delete;
    CompositorThread& operator=(CompositorThread const&) = delete;

    std::unique_ptr<CompositorLoop> compositor_loop;

    CompositorThreadState state;
    std::mutex mutable run_mutex ;
    std::condition_variable run_cv;
    std::condition_variable paused_cv;
    std::thread thread;
};

}
}
#endif
