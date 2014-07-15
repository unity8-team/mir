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
#include "multi_threaded_compositor.h"
#include "gl_renderer_factory.h"
#include "compositing_screencast.h"
#include "timeout_frame_dropping_policy_factory.h"
#include "mir/main_loop.h"

#include "mir/frontend/screencast.h"
#include "mir/options/configuration.h"

#include <boost/throw_exception.hpp>

namespace mc = mir::compositor;
namespace ms = mir::scene;
namespace mf = mir::frontend;

std::shared_ptr<ms::BufferStreamFactory>
mir::DefaultServerConfiguration::the_buffer_stream_factory()
{
    return buffer_stream_factory(
        [this]()
        {
            return std::make_shared<mc::BufferStreamFactory>(the_buffer_allocator(),
                                                             the_frame_dropping_policy_factory());
        });
}

std::shared_ptr<mc::FrameDroppingPolicyFactory>
mir::DefaultServerConfiguration::the_frame_dropping_policy_factory()
{
    return frame_dropping_policy_factory(
        [this]()
        {
            return std::make_shared<mc::TimeoutFrameDroppingPolicyFactory>(the_main_loop(),
                                                                           std::chrono::milliseconds{100});
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
            return std::make_shared<mc::MultiThreadedCompositor>(
                the_display(),
                the_scene(),
                the_display_buffer_compositor_factory(),
                the_compositor_report(),
                !the_options()->is_set(options::host_socket_opt));
        });
}

std::shared_ptr<mc::RendererFactory> mir::DefaultServerConfiguration::the_renderer_factory()
{
    return renderer_factory(
        [this]()
        {
            return std::make_shared<mc::GLRendererFactory>(the_gl_program_factory());
        });
}

std::shared_ptr<mf::Screencast> mir::DefaultServerConfiguration::the_screencast()
{
    return screencast(
        [this]()
        {
            return std::make_shared<mc::CompositingScreencast>(
                the_display(),
                the_buffer_allocator(),
                the_display_buffer_compositor_factory()
                );
        });
}
