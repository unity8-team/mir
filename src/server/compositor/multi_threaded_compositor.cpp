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

#include "multi_threaded_compositor.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "mir/compositor/display_buffer_compositor_factory.h"
#include "mir/compositor/scene.h"
#include "mir/compositor/scene_element.h"
#include "mir/compositor/compositor_report.h"
#include "mir/scene/legacy_scene_change_notification.h"
#include "mir/scene/surface_observer.h"
#include "mir/scene/surface.h"
#include "mir/run_mir.h"
#include "mir/thread_name.h"

#include <thread>
#include <condition_variable>

namespace mc = mir::compositor;
namespace mg = mir::graphics;
namespace ms = mir::scene;

namespace
{

class ApplyIfUnwinding
{
public:
    ApplyIfUnwinding(std::function<void()> const& apply)
        : apply{apply}
    {
    }

    ~ApplyIfUnwinding()
    {
        if (std::uncaught_exception())
            apply();
    }

private:
    ApplyIfUnwinding(ApplyIfUnwinding const&) = delete;
    ApplyIfUnwinding& operator=(ApplyIfUnwinding const&) = delete;

    std::function<void()> const apply;
};

void drop_frame(std::shared_ptr<mc::Scene> const& scene, int nframes)
{
    // Consume each renderable twice using the same compositor_id. This is
    // to ensure the multi-monitor frame sync code doesn't hand the same
    // frame to any real compositor after us.

    int n = 2 * nframes;
    for (int f = 0; f < n; ++f)
    {
        auto const& elements = scene->scene_elements_for(0);
        for (auto& element : elements)
            (void)element->renderable()->buffer();
    }
}

/*
 * When we're waking up from sleep, we will eventually force a new frame
 * to be composited. But we intentionally delay a little, to allow clients
 * and nested servers to produce new frames for us to show on wakeup. Should
 * any client or nested server provide new content during this delay, the
 * delay will be cancelled and a new frame composited immediately.
 */
std::chrono::milliseconds const snooze(500);

/*
 * Annoyingly, we can't use milliseconds::max() because that's actually
 * negative and will trigger immediate wakeups (!?)
 */
std::chrono::milliseconds const forever(INT_MAX);

}

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
                       mg::DisplayBuffer& buffer,
                       std::shared_ptr<CompositorReport> const& report)
        : display_buffer_compositor_factory{db_compositor_factory},
          buffer(buffer),
          running{true},
          frames_scheduled{0},
          timeout{forever},
          report{report}
    {
    }

    void operator()() noexcept  // noexcept is important! (LP: #1237332)
    try
    {
        mir::set_thread_name("Mir/Comp");

        /*
         * Make the buffer the current rendering target, and release
         * it when the thread is finished.
         */
        CurrentRenderingTarget target{buffer};

        auto display_buffer_compositor = display_buffer_compositor_factory->create_compositor_for(buffer);

        CompositorReport::SubCompositorId report_id =
            display_buffer_compositor.get();

        const auto& r = buffer.view_area();
        report->added_display(r.size.width.as_int(), r.size.height.as_int(),
                              r.top_left.x.as_int(), r.top_left.y.as_int(),
                              report_id);

        std::unique_lock<std::mutex> lock{run_mutex};
        while (running)
        {
            while (!frames_scheduled && running)
            {
                auto status = run_cv.wait_for(lock, timeout);
                if (!frames_scheduled && status == std::cv_status::timeout)
                    frames_scheduled = 1;
            }

            timeout = forever;

            /*
             * Check if we are running before compositing, since we may have
             * been stopped while waiting for the run_cv above.
             */
            if (running)
            {
                /*
                 * Each surface could have a number of frames ready in its buffer
                 * queue. And we need to ensure that we render all of them so that
                 * none linger in the queue indefinitely (seen as input lag).
                 * frames_scheduled indicates the number of frames that are scheduled
                 * to ensure all surfaces' queues are fully drained.
                 */
                frames_scheduled--;
                lock.unlock();

                display_buffer_compositor->composite();

                lock.lock();
            }
        }
    }
    catch(...)
    {
        mir::terminate_with_current_exception();
    }

    void schedule_compositing(int num_frames)
    {
        std::lock_guard<std::mutex> lock{run_mutex};

        if (num_frames < 0)
        {
            timeout = snooze;
            num_frames = 0;
        }

        if (num_frames > frames_scheduled)
            frames_scheduled = num_frames;

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
    std::chrono::milliseconds timeout;
    std::mutex run_mutex;
    std::condition_variable run_cv;
    std::shared_ptr<CompositorReport> const report;
};

}
}

mc::MultiThreadedCompositor::MultiThreadedCompositor(
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mc::Scene> const& scene,
    std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory,
    std::shared_ptr<CompositorReport> const& compositor_report,
    bool compose_on_start)
    : display{display},
      scene{scene},
      display_buffer_compositor_factory{db_compositor_factory},
      report{compositor_report},
      state{CompositorState::stopped},
      compose_on_start{compose_on_start}
{
    observer = std::make_shared<ms::LegacySceneChangeNotification>(
    [this]()
    {
        schedule_compositing(1);
    },
    [this](int num)
    {
        schedule_compositing(num);
    });
}

mc::MultiThreadedCompositor::~MultiThreadedCompositor()
{
    stop();
}

void mc::MultiThreadedCompositor::schedule_compositing(int num)
{
    std::unique_lock<std::mutex> lk(state_guard);

    report->scheduled();

    for (auto& f : thread_functors)
        f->schedule_compositing(restarting ? -num : num);

    restarting = false;
}

void mc::MultiThreadedCompositor::start()
{
    std::unique_lock<std::mutex> lk(state_guard);
    if (state != CompositorState::stopped)
    {
        return;
    }

    state = CompositorState::starting;
    report->started();

    /* To cleanup state if any code below throws */
    ApplyIfUnwinding cleanup_if_unwinding{
        [this, &lk]{
            destroy_compositing_threads(lk);
        }};

    lk.unlock();
    scene->add_observer(observer);
    lk.lock();

    create_compositing_threads();

    /* Optional first render */
    if (compose_on_start)
    {
        if (restarting)
        {
            int const max_buffer_queue_depth = 3;
            drop_frame(scene, max_buffer_queue_depth);
        }

        lk.unlock();
        schedule_compositing(1);
    }
}

void mc::MultiThreadedCompositor::stop()
{
    std::unique_lock<std::mutex> lk(state_guard);
    if (state != CompositorState::started)
    {
        return;
    }

    state = CompositorState::stopping;

    /* To cleanup state if any code below throws */
    ApplyIfUnwinding cleanup_if_unwinding{
        [this, &lk]{
            if(!lk.owns_lock()) lk.lock();
            state = CompositorState::started;
        }};

    lk.unlock();
    scene->remove_observer(observer);
    lk.lock();

    destroy_compositing_threads(lk);

    // If the compositor is restarted we've likely got clients blocked
    // so we will need to schedule compositing immediately
    compose_on_start = true;
    restarting = true;
}

void mc::MultiThreadedCompositor::create_compositing_threads()
{
    /* Start the display buffer compositing threads */
    display->for_each_display_buffer([this](mg::DisplayBuffer& buffer)
    {
        auto thread_functor_raw = new mc::CompositingFunctor{display_buffer_compositor_factory, buffer, report};
        auto thread_functor = std::unique_ptr<mc::CompositingFunctor>(thread_functor_raw);

        threads.push_back(std::thread{std::ref(*thread_functor)});
        thread_functors.push_back(std::move(thread_functor));
    });

    state = CompositorState::started;
}

void mc::MultiThreadedCompositor::destroy_compositing_threads(std::unique_lock<std::mutex>& lock)
{
    /* Could be called during unwinding,
     * ensure the lock is held before changing state
     */
    if(!lock.owns_lock())
        lock.lock();

    for (auto& f : thread_functors)
        f->stop();

    for (auto& t : threads)
        t.join();

    thread_functors.clear();
    threads.clear();

    report->stopped();

    state = CompositorState::stopped;
}
