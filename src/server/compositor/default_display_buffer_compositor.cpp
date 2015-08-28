/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 *              Daniel van Vugt <daniel.van.vugt@canonical.com>
 */

#include "default_display_buffer_compositor.h"

#include "mir/compositor/scene.h"
#include "mir/compositor/scene_element.h"
#include "mir/compositor/renderer.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/display_buffer.h"
#include "mir/graphics/buffer.h"
#include "mir/compositor/buffer_stream.h"
#include "occlusion.h"
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>
#include <cstdlib>
#include <algorithm>

namespace mc = mir::compositor;
namespace mg = mir::graphics;

namespace
{

class ReplyThread
{
public:
    ReplyThread();
    ~ReplyThread();
    void handoff(mg::RenderableList &&list);
private:
    void body();
    bool running;
    std::thread th;
    std::mutex mutex;
    std::condition_variable cond;
    std::deque<mg::RenderableList> todo;
};

ReplyThread::ReplyThread() : running(true), th(&ReplyThread::body, this)
{
}

ReplyThread::~ReplyThread()
{
    running = false;
    cond.notify_one();
    th.join();
}

void ReplyThread::body()
{
    std::unique_lock<std::mutex> lock(mutex);
    
    while (running)
    {
        while (running && todo.empty())
            cond.wait(lock);

        while (!todo.empty())
        {
            {
                auto job = std::move(todo.front());
                todo.pop_front();
                lock.unlock();
                // Replies are transparently sent to clients here, which takes
                // significant time. We are not locked while this happens.
            }

            lock.lock();
        }
    }
}

void ReplyThread::handoff(mg::RenderableList &&list)
{
    std::lock_guard<std::mutex> lock(mutex);
    todo.push_back(std::move(list));
    cond.notify_one();
}

ReplyThread reply_thread;

} // anonymous namespace

mc::DefaultDisplayBufferCompositor::DefaultDisplayBufferCompositor(
    mg::DisplayBuffer& display_buffer,
    std::shared_ptr<mc::Renderer> const& renderer,
    std::shared_ptr<mc::CompositorReport> const& report) :
    display_buffer(display_buffer),
    renderer(renderer),
    report(report)
{
}

void mc::DefaultDisplayBufferCompositor::composite(mc::SceneElementSequence&& scene_elements)
{
    report->began_frame(this);

    auto const& view_area = display_buffer.view_area();
    auto const& occlusions = mc::filter_occlusions_from(scene_elements, view_area);

    for (auto const& element : occlusions)
        element->occluded();

    mg::RenderableList renderable_list;
    renderable_list.reserve(scene_elements.size());
    for (auto const& element : scene_elements)
    {
        element->rendered();
        renderable_list.push_back(element->renderable());
    }

    /*
     * Note: Buffer lifetimes are ensured by the two objects holding
     *       references to them; scene_elements and renderable_list.
     *       So no buffer is going to be released back to the client till
     *       both of those containers get destroyed (end of the function).
     *       Actually, there's a third reference held by the texture cache
     *       in GLRenderer, but that gets released earlier in render().
     */
    scene_elements.clear();  // Those in use are still in renderable_list

    if (display_buffer.post_renderables_if_optimizable(renderable_list))
    {
        report->renderables_in_frame(this, renderable_list);
        renderer->suspend();
    }
    else
    {
        display_buffer.make_current();

        renderer->set_rotation(display_buffer.orientation());

        renderer->render(renderable_list);

        display_buffer.gl_swap_buffers();
        report->renderables_in_frame(this, renderable_list);
        report->rendered_frame(this);

        // Release the buffers we did use back to the clients, before starting
        // on the potentially slow swap-buffers and post().
        /*
         * FIXME: Destruction of the renderable_list blocks for a significant
         * time (LP: #1395421). In order to avoid this slowing down our
         * rendering, and particularly to avoid inserting blocking socket
         * calls in here that could cause context switches before we get to
         * the swap buffers, we hand off destruction of the renderable_list
         * to another thread. This is arguably more a workaround than a fix
         * because the ideal solution would be to fix out sockets code to
         * only use asynchronous sends.
         */

        reply_thread.handoff(std::move(renderable_list));
    }

    report->finished_frame(this);
}
