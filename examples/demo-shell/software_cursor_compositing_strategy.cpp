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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "software_cursor_compositing_strategy.h"

#include "mir/compositor/rendering_operator.h"
#include "mir/geometry/rectangle.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "mir/graphics/renderable.h"
#include "mir/graphics/renderer.h"

namespace me = mir::examples;
namespace mc = mir::compositor;
namespace mg = mir::graphics;

me::SoftwareCursorCompositingStrategy::SoftwareCursorCompositingStrategy(std::shared_ptr<mc::Renderables> const& renderables,
    std::shared_ptr<mg::Renderer> const& renderer) :
    renderables(renderables),
    renderer(renderer)
{
}

namespace
{

// TODO: We should share at least this with the DefaultCompositingStrategy. Likely more!
// all we want to do is paint an overlay so it's strange to replace the entire compositor
struct FilterForVisibleRenderablesInRegion : public mc::FilterForRenderables
{
    FilterForVisibleRenderablesInRegion(mir::geometry::Rectangle const& enclosing_region)
        : enclosing_region(enclosing_region)
    {
    }
    bool operator()(mg::Renderable& renderable)
    {
        // TODO check against enclosing_region
        return !renderable.hidden();
    }

    mir::geometry::Rectangle const& enclosing_region;
};

}

void me::SoftwareCursorCompositingStrategy::render(mg::DisplayBuffer& display_buffer)
{
    mc::RenderingOperator applicator(*renderer);
    FilterForVisibleRenderablesInRegion selector(display_buffer.view_area());

    display_buffer.make_current();
    display_buffer.clear();

    renderables->for_each_if(selector, applicator);

    display_buffer.post_update();
}

