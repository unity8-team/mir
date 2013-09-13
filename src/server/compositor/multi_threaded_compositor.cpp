/*
 * Copyright © 2013 Canonical Ltd.
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

#include "mir/compositor/multi_threaded_compositor.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/compositor/scene.h"

#include <thread>
#include <pthread.h>
#include <condition_variable>

namespace mc = mir::compositor;
namespace mg = mir::graphics;

namespace mir
{
namespace compositor
{

class CurrentRenderingTarget
{
public:
    CurrentRenderingTarget(mg::DisplayBuffer& buffer)
        : buffer(buffer)
    {
        buffer.make_current();
    }

    ~CurrentRenderingTarget()
    {
        buffer.release_current();
    }

private:
    mg::DisplayBuffer& buffer;
};

class CompositingFunctor
{
public:
    CompositingFunctor(std::shared_ptr<mc::DisplayBufferCompositorFactory> const& db_compositor_factory,
                       mg::DisplayBuffer& buffer)
        : display_buffer_compositor_factory{db_compositor_factory},
          buffer(buffer),
          running{true},
          frames_scheduled{0}
    {
    }

    void operator()()
    {
        pthread_setname_np(pthread_self(), "compositor");
        std::unique_lock<std::mutex> lock{run_mutex};

        /*
         * Make the buffer the current rendering target, and release
         * it when the thread is finished.
         */
        CurrentRenderingTarget target{buffer};

        auto display_buffer_compositor = display_buffer_compositor_factory->create_compositor_for(buffer);

        while (running)
        {
            /* Wait until compositing has been scheduled or we are stopped */
            while (!frames_scheduled && running)
                run_cv.wait(lock);

            frames_scheduled--;

            /*
             * Check if we are running before compositing, since we may have
             * been stopped while waiting for the run_cv above.
             */
            if (running)
            {
                lock.unlock();
                display_buffer_compositor->composite();
                lock.lock();
            }
        }
    }

    void schedule_compositing()
    {
        std::lock_guard<std::mutex> lock{run_mutex};

        /*
         * Each surface could have a number of frames ready in its buffer
         * queue. And we need to ensure that we render all of them so that
         * none linger in the queue indefinitely (seen as input lag). So while
         * there's no API support for finding out queue lengths, assume the
         * worst and schedule enough frames to ensure all surfaces' queues
         * are fully drained.
         */
        frames_scheduled = max_client_buffers;
        run_cv.notify_one();
    }

    void stop()
    {
        std::lock_guard<std::mutex> lock{run_mutex};
        running = false;
        run_cv.notify_one();
    }

private:
    std::shared_ptr<mc::DisplayBufferCompositorFactory> const display_buffer_compositor_factory;
    mg::DisplayBuffer& buffer;
    bool running;
    int frames_scheduled;
    std::mutex run_mutex;
    std::condition_variable run_cv;
};

}
}

mc::MultiThreadedCompositor::MultiThreadedCompositor(
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mc::Scene> const& scene,
    std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory)
    : display{display},
      scene{scene},
      display_buffer_compositor_factory{db_compositor_factory}
{
}

mc::MultiThreadedCompositor::~MultiThreadedCompositor()
{
    stop();
}

void mc::MultiThreadedCompositor::start()
{
    /* Start the compositing threads */
    display->for_each_display_buffer([this](mg::DisplayBuffer& buffer)
    {
        auto thread_functor_raw = new mc::CompositingFunctor{display_buffer_compositor_factory, buffer};
        auto thread_functor = std::unique_ptr<mc::CompositingFunctor>(thread_functor_raw);

        threads.push_back(std::thread{std::ref(*thread_functor)});
        thread_functors.push_back(std::move(thread_functor));
    });

    /* Recomposite whenever the scene changes */
    scene->set_change_callback([this]()
    {
        for (auto& f : thread_functors)
            f->schedule_compositing();
    });

    /* First render */
    for (auto& f : thread_functors)
        f->schedule_compositing();
}

void mc::MultiThreadedCompositor::stop()
{
    scene->set_change_callback([]{});

    for (auto& f : thread_functors)
        f->stop();

    for (auto& t : threads)
        t.join();

    thread_functors.clear();
    threads.clear();
}
