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

#ifndef MIR_COMPOSITOR_MULTI_THREADED_COMPOSITOR_H_
#define MIR_COMPOSITOR_MULTI_THREADED_COMPOSITOR_H_

#include "mir/compositor/compositor.h"

#include <memory>
#include <vector>
#include <thread>
#include <mutex>

namespace mir
{
namespace graphics
{
class Display;
}
namespace compositor
{

enum {max_client_buffers = 3};

class DisplayBufferCompositorFactory;
class CompositingFunctor;
class Scene;

class MultiThreadedCompositor : public Compositor
{
public:
    MultiThreadedCompositor(std::shared_ptr<graphics::Display> const& display,
                            std::shared_ptr<Scene> const& scene,
                            std::shared_ptr<DisplayBufferCompositorFactory> const& db_compositor_factory);
    ~MultiThreadedCompositor();

    void start();
    void stop();
    
    void while_pausing_composition(std::function<void()> const& exec);

private:
    std::shared_ptr<graphics::Display> const display;
    std::shared_ptr<Scene> const scene;
    std::shared_ptr<DisplayBufferCompositorFactory> const display_buffer_compositor_factory;

    std::vector<std::unique_ptr<CompositingFunctor>> thread_functors;
    std::vector<std::thread> threads;
    
    std::mutex threads_guard;
    
    bool running();
};

}
}

#endif /* MIR_COMPOSITOR_MULTI_THREADED_COMPOSITOR_H_ */
