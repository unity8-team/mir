/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
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

#include "mir/server_configuration.h"

#include "mir/options/program_option.h"
#include "mir/compositor/buffer_allocation_strategy.h"
#include "mir/compositor/buffer_swapper.h"
#include "mir/compositor/buffer_bundle_manager.h"
#include "mir/compositor/double_buffer_allocation_strategy.h"
#include "mir/frontend/protobuf_ipc_factory.h"
#include "mir/frontend/application_listener.h"
#include "mir/frontend/application_proxy.h"
#include "mir/frontend/resource_cache.h"
#include "mir/frontend/application_manager.h"
#include "mir/frontend/registration_order_focus_selection_strategy.h"
#include "mir/frontend/single_visibility_focus_mechanism.h"
#include "mir/frontend/application_session_model.h"
#include "mir/graphics/display.h"
#include "mir/graphics/gl_renderer.h"
#include "mir/graphics/renderer.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/buffer_initializer.h"
#include "mir/input/input_manager.h"
#include "mir/logging/logger.h"
#include "mir/logging/dumb_console_logger.h"
#include "mir/surfaces/surface_controller.h"
#include "mir/surfaces/surface_stack.h"

namespace mc = mir::compositor;
namespace geom = mir::geometry;
namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace ml = mir::logging;
namespace ms = mir::surfaces;
namespace mi = mir::input;

namespace
{
class DefaultIpcFactory : public mf::ProtobufIpcFactory
{
public:
    explicit DefaultIpcFactory(
        std::shared_ptr<mf::ApplicationSessionFactory> const& application_session_factory,
        std::shared_ptr<mf::ApplicationListener> const& listener,
        std::shared_ptr<mg::Platform> const& graphics_platform,
        std::shared_ptr<mg::Display> const& graphics_display) :
        application_session_factory(application_session_factory),
        listener(listener),
        cache(std::make_shared<mf::ResourceCache>()),
        graphics_platform(graphics_platform),
        graphics_display(graphics_display)
    {
    }

private:
    std::shared_ptr<mf::ApplicationSessionFactory> application_session_factory;
    std::shared_ptr<mf::ApplicationListener> const listener;
    std::shared_ptr<mf::ResourceCache> const cache;
    std::shared_ptr<mg::Platform> const graphics_platform;
    std::shared_ptr<mg::Display> const graphics_display;

    virtual std::shared_ptr<mir::protobuf::DisplayServer> make_ipc_server()
    {
        return std::make_shared<mf::ApplicationProxy>(
            application_session_factory,
            graphics_platform,
            graphics_display,
            listener,
            resource_cache());
    }

    virtual std::shared_ptr<mf::ResourceCache> resource_cache()
    {
        return cache;
    }
};
}

mir::DefaultServerConfiguration::DefaultServerConfiguration(std::string const& socket_file) :
    socket_file(socket_file)
{
}

std::shared_ptr<mir::options::Option> mir::DefaultServerConfiguration::make_options()
{
    if (!options)
    {
        namespace po = boost::program_options;

        po::options_description desc("Environment options");
        desc.add_options()
            ("android_sdk_dir", po::value<std::string>(), "dummy")
            ("android_ndk_dir", po::value<std::string>(), "dummy")
            ("tests_use_real_graphics", po::value<bool>(), "use real graphics in tests")
            ("tests_use_real_input", po::value<bool>(), "use real input in tests");
        
        auto options = std::make_shared<mir::options::ProgramOption>();

        options->parse_environment(desc, "MIR_");

        this->options = options;
    }

    return options;
}

std::shared_ptr<mg::Platform> mir::DefaultServerConfiguration::make_graphics_platform()
{
    if (!graphics_platform)
    {
        // TODO I doubt we need the extra level of indirection provided by
        // mg::create_platform() - we just need to move the implementation
        // of DefaultServerConfiguration::make_graphics_platform() to the
        // graphics libraries.
        graphics_platform = mg::create_platform();
    }

    return graphics_platform;
}

std::shared_ptr<mg::BufferInitializer>
mir::DefaultServerConfiguration::make_buffer_initializer()
{
    return std::make_shared<mg::NullBufferInitializer>();
}

std::shared_ptr<mc::BufferAllocationStrategy>
mir::DefaultServerConfiguration::make_buffer_allocation_strategy(
        std::shared_ptr<mc::GraphicBufferAllocator> const& buffer_allocator)
{
    return std::make_shared<mc::DoubleBufferAllocationStrategy>(buffer_allocator);
}

std::shared_ptr<mg::Renderer> mir::DefaultServerConfiguration::make_renderer(
        std::shared_ptr<mg::Display> const& display)
{
    return std::make_shared<mg::GLRenderer>(display->view_area().size);
}

std::shared_ptr<mf::ApplicationSessionFactory>
mir::DefaultServerConfiguration::make_application_session_factory(std::shared_ptr<ms::ApplicationSurfaceOrganiser> const& surface_organiser)
{
    auto session_model = std::make_shared<mf::ApplicationSessionModel>();
    auto focus_mechanism = std::make_shared<mf::SingleVisibilityFocusMechanism>(session_model);
    auto focus_selection_strategy = std::make_shared<mf::RegistrationOrderFocusSelectionStrategy>(session_model);
    return std::make_shared<mf::ApplicationManager>(surface_organiser, session_model, focus_selection_strategy, focus_mechanism);
}

std::shared_ptr<mi::InputManager>
mir::DefaultServerConfiguration::make_input_manager(
    const std::initializer_list<std::shared_ptr<mi::EventFilter> const>& event_filters,
    std::shared_ptr<mg::ViewableArea> const& view_area)
{
    return mi::create_input_manager(event_filters, view_area);
}

std::shared_ptr<mir::frontend::ProtobufIpcFactory>
mir::DefaultServerConfiguration::make_ipc_factory(
    std::shared_ptr<mf::ApplicationSessionFactory> const& application_session_factory,
    std::shared_ptr<mg::Display> const& display)
{
    return std::make_shared<DefaultIpcFactory>(
        application_session_factory,
        make_application_listener(),
        make_graphics_platform(),
        display);
}

std::shared_ptr<mf::ApplicationListener>
mir::DefaultServerConfiguration::make_application_listener()
{
    return std::make_shared<mf::NullApplicationListener>();
}
