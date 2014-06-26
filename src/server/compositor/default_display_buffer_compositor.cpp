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
#include <cstdlib>
#include <algorithm>

namespace mc = mir::compositor;
namespace mg = mir::graphics;

mc::DefaultDisplayBufferCompositor::DefaultDisplayBufferCompositor(
    mg::DisplayBuffer& display_buffer,
    std::shared_ptr<mc::Scene> const& scene,
    std::shared_ptr<mc::Renderer> const& renderer,
    std::shared_ptr<mc::CompositorReport> const& report)
    : display_buffer(display_buffer),
      scene{scene},
      renderer{renderer},
      report{report}
{
    scene->register_compositor(this);
}

mc::DefaultDisplayBufferCompositor::~DefaultDisplayBufferCompositor()
{
    scene->unregister_compositor(this);
}

void mc::DefaultDisplayBufferCompositor::composite()
{
    report->began_frame(this);

    auto const& view_area = display_buffer.view_area();
    auto scene_elements = scene->scene_elements_for(this);
    auto const& occlusions = mc::filter_occlusions_from(scene_elements, view_area);

    for (auto const& element : occlusions)
    {
        if (element->renderable()->visible())
            element->occluded_in(this);
    }

    mg::RenderableList renderable_list;
    for (auto const& element : scene_elements)
    {
        element->rendered_in(this);
        renderable_list.push_back(element->renderable());
    }

    if (display_buffer.post_renderables_if_optimizable(renderable_list))
    {
        renderer->suspend();
        report->finished_frame(true, this);
    }
    else
    {
        display_buffer.make_current();

        renderer->set_rotation(display_buffer.orientation());

        renderer->begin();  // TODO deprecatable now?
        renderer->render(renderable_list);
        display_buffer.post_update();
        renderer->end();

        report->finished_frame(false, this);
    }
}
