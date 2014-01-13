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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */

#include "mir/default_server_configuration.h"
#include "buffer_stream_factory.h"
#include "default_display_buffer_compositor_factory.h"
#include "gl_renderer_factory.h"
#include "multi_threaded_compositor.h"
#include "renderer.h"

#include <boost/throw_exception.hpp>
#include <stdexcept>
#include <sstream>

namespace mc = mir::compositor;
namespace ms = mir::scene;

namespace
{

float clamp(unsigned long color)
{
    if (color > 255) color = 255;
    return float(color)/255.0f;
}

mc::Renderer::Color extract_color(std::string const& value)
{
    unsigned long color;
    std::istringstream in(value);
    in >> std::hex >> color;

    if (in.fail() || !(in >> std::ws).eof())
        BOOST_THROW_EXCEPTION(std::runtime_error("Clear color should be a hexadecmial number"));

    return mc::Renderer::Color
        {
            clamp((color >> 24) & 0xFF),
            clamp((color >> 16) & 0xFF),
            clamp((color >> 8)  & 0xFF),
            clamp(color         & 0xFF)
        };
}

}

std::shared_ptr<ms::BufferStreamFactory>
mir::DefaultServerConfiguration::the_buffer_stream_factory()
{
    return buffer_stream_factory(
        [this]()
        {
            return std::make_shared<mc::BufferStreamFactory>(the_buffer_allocator());
        });
}

std::shared_ptr<mc::DisplayBufferCompositorFactory>
mir::DefaultServerConfiguration::the_display_buffer_compositor_factory()
{
    return display_buffer_compositor_factory(
        [this]()
        {
            return std::make_shared<mc::DefaultDisplayBufferCompositorFactory>(
                the_scene(), the_renderer_factory(), the_compositor_report());
        });
}

std::shared_ptr<mc::Compositor>
mir::DefaultServerConfiguration::the_compositor()
{
    return compositor(
        [this]()
        {
            return std::make_shared<mc::MultiThreadedCompositor>(the_display(),
                                                                 the_scene(),
                                                                 the_display_buffer_compositor_factory(),
                                                                 the_compositor_report());
        });
}

std::shared_ptr<mc::RendererFactory> mir::DefaultServerConfiguration::the_renderer_factory()
{
    struct ClearColorSettingRendererFactory : mc::GLRendererFactory
    {
        ClearColorSettingRendererFactory(mc::Renderer::Color const& color) : color(color) {}

        std::unique_ptr<mc::Renderer> create_renderer_for(geometry::Rectangle const& rect) override
        {
            std::unique_ptr<mc::Renderer> renderer{GLRendererFactory::create_renderer_for(rect)};
            renderer->set_clear_color(color);
            return renderer;
        }

        mc::Renderer::Color color;
    };

    return renderer_factory(
        [this]() -> std::shared_ptr<mc::RendererFactory>
        {
            auto const options = the_options();
            if (options->is_set(clear_color))
            {
                mc::Renderer::Color color = extract_color(options->get(clear_color, "0x000000FF"));
                return std::make_shared<ClearColorSettingRendererFactory>(color);
            }
            return std::make_shared<mc::GLRendererFactory>();
        });
}
