/*
 * Copyright © 2016 Canonical Ltd.
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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include "mir_test_framework/headless_display_buffer_compositor_factory.h"
#include "mir/renderer/gl/render_target.h"
#include "mir/graphics/display_buffer.h"
#include "mir/compositor/display_buffer_compositor.h"
#include "mir/compositor/scene_element.h"
#include "mir/geometry/rectangle.h"
#include <algorithm>

namespace mtf = mir_test_framework;
namespace mg = mir::graphics;
namespace mrg = mir::renderer::gl;
namespace mc = mir::compositor;
namespace geom = mir::geometry;

std::unique_ptr<mir::compositor::DisplayBufferCompositor>
mtf::HeadlessDisplayBufferCompositorFactory::create_compositor_for(mg::DisplayBuffer& db)
{
    struct HeadlessDBC : mir::compositor::DisplayBufferCompositor
    {
        HeadlessDBC(mg::DisplayBuffer& db) :
            db(db),
            render_target(dynamic_cast<mrg::RenderTarget*>(
                db.native_display_buffer()))
        {
        }

        mg::RenderableList filter(
            mc::SceneElementSequence& elements,
            geom::Rectangle const& display_area)
        {
            mg::RenderableList renderables;
            for (auto it = elements.rbegin(); it != elements.rend(); it++)
            {
                auto const area = (*it)->renderable()->screen_position().intersection_with(display_area);
                bool offscreen = (area == geom::Rectangle{});
                bool occluded = false;
                for(auto& r : renderables)
                    occluded |= r->screen_position().contains(area) && !r->shaped() && (r->alpha() == 1.0f);

                if (offscreen || occluded)
                {
                    (*it)->occluded();
                }
                else
                {
                    (*it)->rendered();
                    renderables.push_back((*it)->renderable());
                }
            }
            std::reverse(renderables.begin(), renderables.end());
            return renderables;
        }

        void composite(mir::compositor::SceneElementSequence&& seq) override
        {
            auto renderlist = filter(seq, db.view_area());
            if (db.post_renderables_if_optimizable(renderlist))
                return;

            // Invoke GL renderer specific functions if the DisplayBuffer supports them
            if (render_target)
                render_target->make_current();

            // We need to consume a buffer to unblock client tests
            for (auto const& renderable : renderlist)
                renderable->buffer(); 

            if (render_target)
                render_target->swap_buffers();
        }
        mg::DisplayBuffer& db;
        mrg::RenderTarget* const render_target;
    };
    return std::make_unique<HeadlessDBC>(db);
}
