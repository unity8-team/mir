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
 */

#ifndef MIR_COMPOSITOR_DEFAULT_DISPLAY_BUFFER_COMPOSITOR_FACTORY_H_
#define MIR_COMPOSITOR_DEFAULT_DISPLAY_BUFFER_COMPOSITOR_FACTORY_H_

#include "mir/compositor/display_buffer_compositor_factory.h"

namespace mir
{
///  Compositing. Combining renderables into a display image.
namespace compositor
{
class RendererFactory;
class OverlayRenderer;
class Scene;

// TODO this is ideally an implementation class. It is only in a public header
// TODO because it is used in some example code (which probably needs rethinking).
class DefaultDisplayBufferCompositorFactory : public DisplayBufferCompositorFactory
{
public:
    DefaultDisplayBufferCompositorFactory(
        std::shared_ptr<Scene> const& scene,
        std::shared_ptr<RendererFactory> const& renderer_factory,
        std::shared_ptr<OverlayRenderer> const& overlay_renderer);

    std::unique_ptr<DisplayBufferCompositor> create_compositor_for(graphics::DisplayBuffer& display_buffer);

private:
    std::shared_ptr<Scene> const scene;
    std::shared_ptr<RendererFactory> const renderer_factory;
    std::shared_ptr<OverlayRenderer> const overlay_renderer;
};

}
}

#endif /* MIR_COMPOSITOR_DEFAULT_DISPLAY_BUFFER_COMPOSITOR_FACTORY_H_ */
