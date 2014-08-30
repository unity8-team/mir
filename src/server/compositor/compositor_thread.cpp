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

#include "compositor_thread.h"
#include "mir/run_mir.h"
#include "mir/thread_name.h"

#include <boost/throw_exception.hpp>

namespace mc = mir::compositor;

mc::CompositorThread::CompositorThread(std::unique_ptr<mc::CompositorLoop> loop)
    : compositor_loop{std::move(loop)},
      state{CompositorThreadState::running},
      thread{&mc::CompositorThread::thread_entry, this}
{
}

mc::CompositorThread::~CompositorThread()
{
    stop();
    if (thread.joinable())
        thread.join();
}

void mc::CompositorThread::thread_entry() noexcept  // noexcept is important! (LP: #1237332)
try
{
   mir::set_thread_name("Mir/Comp");

   std::unique_lock<std::mutex> lock{run_mutex};
   while (state != CompositorThreadState::stopping)
   {
       run_cv.wait(lock, [&]{ return state !=  CompositorThreadState::paused; });

       if (state == CompositorThreadState::running)
       {
           lock.unlock();
           compositor_loop->run();
           lock.lock();
       }

       if (state == CompositorThreadState::pausing)
       {
           state = CompositorThreadState::paused;
           paused_cv.notify_all();
       }
   }
}
catch(...)
{
    std::lock_guard<std::mutex> lock{run_mutex};
    state = CompositorThreadState::stopping;
    compositor_loop = nullptr;
    paused_cv.notify_all();

    mir::terminate_with_current_exception();
}

void mc::CompositorThread::run(std::unique_ptr<CompositorLoop> loop)
{
    std::lock_guard<std::mutex> lock{run_mutex};

    if (state == CompositorThreadState::running)
    {
        BOOST_THROW_EXCEPTION(std::logic_error("Another compositor loop is already running!"));
    }

    compositor_loop = std::move(loop);

    state = CompositorThreadState::running;
    run_cv.notify_one();
}

void mc::CompositorThread::pause()
{
    std::unique_lock<std::mutex> lock{run_mutex};
    pause(lock);
}

void mc::CompositorThread::stop()
{
    std::lock_guard<std::mutex> lock{run_mutex};

    state = CompositorThreadState::stopping;

    if (compositor_loop)
        compositor_loop->stop();

    run_cv.notify_one();
}

void mc::CompositorThread::schedule_compositing(int num_frames)
{
    std::lock_guard<std::mutex> lock{run_mutex};

    if (state == CompositorThreadState::running)
    {
        compositor_loop->schedule_compositing(num_frames);
    }
}

bool mc::CompositorThread::is_running() const
{
    std::lock_guard<std::mutex> lock{run_mutex};
    return state == CompositorThreadState::running;
}

void mc::CompositorThread::pause(std::unique_lock<std::mutex>& lock)
{
    if (state != CompositorThreadState::running)
    {
        return;
    }

    state = mc::CompositorThreadState::pausing;
    compositor_loop->stop();
    run_cv.notify_one();

    paused_cv.wait(lock, [&]{
        return state == CompositorThreadState::paused ||
               state == CompositorThreadState::stopping;
    });

    compositor_loop = nullptr;
}
