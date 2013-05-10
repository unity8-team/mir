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

#include "mir/default_server_configuration.h"
#include "mir/abnormal_exit.h"
#include "mir/asio_main_loop.h"

#include "mir/options/program_option.h"
#include "mir/compositor/buffer_allocation_strategy.h"
#include "mir/compositor/buffer_swapper.h"
#include "mir/compositor/buffer_bundle_manager.h"
#include "mir/compositor/default_compositing_strategy.h"
#include "mir/compositor/multi_threaded_compositor.h"
#include "mir/compositor/swapper_factory.h"
#include "mir/compositor/overlay_renderer.h"
#include "mir/frontend/protobuf_ipc_factory.h"
#include "mir/frontend/session_mediator_report.h"
#include "mir/frontend/null_message_processor_report.h"
#include "mir/frontend/session_mediator.h"
#include "mir/frontend/resource_cache.h"
#include "mir/shell/session_manager.h"
#include "mir/shell/registration_order_focus_sequence.h"
#include "mir/shell/single_visibility_focus_mechanism.h"
#include "mir/shell/default_session_container.h"
#include "mir/shell/consuming_placement_strategy.h"
#include "mir/shell/organising_surface_factory.h"
#include "mir/graphics/cursor.h"
#include "mir/shell/null_session_listener.h"
#include "mir/graphics/display.h"
#include "mir/graphics/gl_renderer.h"
#include "mir/graphics/renderer.h"
#include "mir/graphics/platform.h"
#include "mir/graphics/buffer_initializer.h"
#include "mir/graphics/null_display_report.h"
#include "mir/input/cursor_listener.h"
#include "mir/input/null_input_manager.h"
#include "mir/input/null_input_target_listener.h"
#include "input/android/default_android_input_configuration.h"
#include "input/android/android_input_manager.h"
#include "input/android/android_dispatcher_controller.h"
#include "mir/logging/logger.h"
#include "mir/logging/input_report.h"
#include "mir/logging/dumb_console_logger.h"
#include "mir/logging/glog_logger.h"
#include "mir/logging/session_mediator_report.h"
#include "mir/logging/message_processor_report.h"
#include "mir/logging/display_report.h"
#include "mir/shell/surface_source.h"
#include "mir/surfaces/surface_stack.h"
#include "mir/surfaces/surface_controller.h"
#include "mir/time/high_resolution_clock.h"
#include "mir/default_configuration.h"

namespace mc = mir::compositor;
namespace me = mir::events;
namespace geom = mir::geometry;
namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace ml = mir::logging;
namespace ms = mir::surfaces;
namespace msh = mir::shell;
namespace mi = mir::input;
namespace mia = mi::android;

namespace
{
std::initializer_list<std::shared_ptr<mi::EventFilter> const> empty_filter_list{};
}

namespace
{
class DefaultIpcFactory : public mf::ProtobufIpcFactory
{
public:
    explicit DefaultIpcFactory(
        std::shared_ptr<mf::Shell> const& shell,
        std::shared_ptr<mf::SessionMediatorReport> const& sm_report,
        std::shared_ptr<mf::MessageProcessorReport> const& mr_report,
        std::shared_ptr<mg::Platform> const& graphics_platform,
        std::shared_ptr<mg::ViewableArea> const& graphics_display,
        std::shared_ptr<mc::GraphicBufferAllocator> const& buffer_allocator) :
        shell(shell),
        sm_report(sm_report),
        mp_report(mr_report),
        cache(std::make_shared<mf::ResourceCache>()),
        graphics_platform(graphics_platform),
        graphics_display(graphics_display),
        buffer_allocator(buffer_allocator)
    {
    }

private:
    std::shared_ptr<mf::Shell> shell;
    std::shared_ptr<mf::SessionMediatorReport> const sm_report;
    std::shared_ptr<mf::MessageProcessorReport> const mp_report;
    std::shared_ptr<mf::ResourceCache> const cache;
    std::shared_ptr<mg::Platform> const graphics_platform;
    std::shared_ptr<mg::ViewableArea> const graphics_display;
    std::shared_ptr<mc::GraphicBufferAllocator> const buffer_allocator;

    virtual std::shared_ptr<mir::protobuf::DisplayServer> make_ipc_server(
        std::shared_ptr<me::EventSink> const& sink)
    {
        return std::make_shared<mf::SessionMediator>(
            shell,
            graphics_platform,
            graphics_display,
            buffer_allocator,
            sm_report,
            sink,
            resource_cache());
    }

    virtual std::shared_ptr<mf::ResourceCache> resource_cache()
    {
        return cache;
    }

    virtual std::shared_ptr<mf::MessageProcessorReport> report()
    {
        return mp_report;
    }
};

char const* const log_app_mediator = "log-app-mediator";
char const* const log_msg_processor = "log-msg-processor";
char const* const log_display      = "log-display";
char const* const log_input        = "log-input";

char const* const glog                 = "glog";
char const* const glog_stderrthreshold = "glog-stderrthreshold";
char const* const glog_minloglevel     = "glog-minloglevel";
char const* const glog_log_dir         = "glog-log-dir";

bool const enable_input_default = true;

void parse_arguments(
    boost::program_options::options_description desc,
    std::shared_ptr<mir::options::ProgramOption> const& options,
    int argc,
    char const* argv[])
{
    namespace po = boost::program_options;

    bool exit_with_helptext = false;

    try
    {
        desc.add_options()
            ("help,h", "this help text");

        options->parse_arguments(desc, argc, argv);

        if (options->is_set("help"))
        {
            exit_with_helptext = true;
        }
    }
    catch (po::error const& error)
    {
        exit_with_helptext = true;
    }

    if (exit_with_helptext)
    {
        std::ostringstream help_text;
        help_text << desc;

        BOOST_THROW_EXCEPTION(mir::AbnormalExit(help_text.str()));
    }
}

void parse_environment(
    boost::program_options::options_description& desc,
    std::shared_ptr<mir::options::ProgramOption> const& options)
{
    options->parse_environment(desc, "MIR_SERVER_");
}
}

mir::DefaultServerConfiguration::DefaultServerConfiguration(int argc, char const* argv[]) :
    argc(argc),
    argv(argv),
    program_options(std::make_shared<boost::program_options::options_description>(
    "Command-line options.\n"
    "Environment variables capitalise long form with prefix \"MIR_SERVER_\" and \"_\" in place of \"-\""))
{
    namespace po = boost::program_options;

    add_options()
        ("file,f", po::value<std::string>(),    "Socket filename")
        ("enable-input,i", po::value<bool>(),   "Enable input. [bool:default=true]")
        (log_display, po::value<bool>(),        "Log the Display report. [bool:default=false]")
        (log_input, po::value<bool>(),          "Log the input stack. [bool:default=false]")
        (log_app_mediator, po::value<bool>(),   "Log the ApplicationMediator report. [bool:default=false]")
        (log_msg_processor, po::value<bool>(), "log the MessageProcessor report")
        (glog,                                  "Use google::GLog for logging")
        (glog_stderrthreshold, po::value<int>(),"Copy log messages at or above this level "
                                                "to stderr in addition to logfiles. The numbers "
                                                "of severity levels INFO, WARNING, ERROR, and "
                                                "FATAL are 0, 1, 2, and 3, respectively."
                                                " [int:default=2]")
        (glog_minloglevel, po::value<int>(),    "Log messages at or above this level. The numbers "
                                                "of severity levels INFO, WARNING, ERROR, and "
                                                "FATAL are 0, 1, 2, and 3, respectively."
                                                " [int:default=0]")
        (glog_log_dir, po::value<std::string>(),"If specified, logfiles are written into this "
                                                "directory instead of the default logging directory."
                                                " [string:default=\"\"]")
        ("ipc-thread-pool", po::value<int>(),   "threads in frontend thread pool. [int:default=10]");
}

boost::program_options::options_description_easy_init mir::DefaultServerConfiguration::add_options()
{
    if (options)
        BOOST_THROW_EXCEPTION(std::logic_error("add_options() must be called before the_options()"));

    return program_options->add_options();
}

std::string mir::DefaultServerConfiguration::the_socket_file() const
{
    return the_options()->get("file", mir::default_server_socket);
}

std::shared_ptr<mir::options::Option> mir::DefaultServerConfiguration::the_options() const
{
    if (!options)
    {
        auto options = std::make_shared<mir::options::ProgramOption>();

        parse_arguments(*program_options, options, argc, argv);
        parse_environment(*program_options, options);

        this->options = options;
    }
    return options;
}

std::shared_ptr<mg::DisplayReport> mir::DefaultServerConfiguration::the_display_report()
{
    return display_report(
        [this]() -> std::shared_ptr<graphics::DisplayReport>
        {
            if (the_options()->get(log_display, false))
            {
                return std::make_shared<ml::DisplayReport>(the_logger());
            }
            else
            {
                return std::make_shared<mg::NullDisplayReport>();
            }
        });
}

std::shared_ptr<mg::Platform> mir::DefaultServerConfiguration::the_graphics_platform()
{
    return graphics_platform(
        [this]()
        {
            // TODO I doubt we need the extra level of indirection provided by
            // mg::create_platform() - we just need to move the implementation
            // of DefaultServerConfiguration::the_graphics_platform() to the
            // graphics libraries.
            // Alternatively, if we want to dynamically load the graphics library
            // then this would be the place to do that.
            return mg::create_platform(the_display_report());
        });
}

std::shared_ptr<mg::BufferInitializer>
mir::DefaultServerConfiguration::the_buffer_initializer()
{
    return buffer_initializer(
        []()
        {
             return std::make_shared<mg::NullBufferInitializer>();
        });
}

std::shared_ptr<mc::BufferAllocationStrategy>
mir::DefaultServerConfiguration::the_buffer_allocation_strategy()
{
    return buffer_allocation_strategy(
        [this]()
        {
             return std::make_shared<mc::SwapperFactory>(the_buffer_allocator());
        });
}

std::shared_ptr<mg::Renderer> mir::DefaultServerConfiguration::the_renderer()
{
    return renderer(
        [&]()
        {
             return std::make_shared<mg::GLRenderer>(the_display()->view_area().size);
        });
}

std::shared_ptr<msh::SessionContainer>
mir::DefaultServerConfiguration::the_shell_session_container()
{
    return shell_session_container(
        []{ return std::make_shared<msh::DefaultSessionContainer>(); });
}

std::shared_ptr<msh::FocusSetter>
mir::DefaultServerConfiguration::the_shell_focus_setter()
{
    return shell_focus_setter(
        [this]
        {
            return std::make_shared<msh::SingleVisibilityFocusMechanism>(
                the_shell_session_container());
        });
}

std::shared_ptr<msh::FocusSequence>
mir::DefaultServerConfiguration::the_shell_focus_sequence()
{
    return shell_focus_sequence(
        [this]
        {
            return std::make_shared<msh::RegistrationOrderFocusSequence>(
                the_shell_session_container());
        });
}

std::shared_ptr<msh::PlacementStrategy>
mir::DefaultServerConfiguration::the_shell_placement_strategy()
{
    return shell_placement_strategy(
        [this]
        {
            return std::make_shared<msh::ConsumingPlacementStrategy>(the_display());
        });
}

std::shared_ptr<msh::SessionListener>
mir::DefaultServerConfiguration::the_shell_session_listener()
{
    return shell_session_listener(
        [this]
        {
            return std::make_shared<msh::NullSessionListener>();
        });
}

std::shared_ptr<msh::SessionManager>
mir::DefaultServerConfiguration::the_session_manager()
{
    return session_manager(
        [this]() -> std::shared_ptr<msh::SessionManager>
        {
            return std::make_shared<msh::SessionManager>(
                the_shell_surface_factory(),
                the_shell_session_container(),
                the_shell_focus_sequence(),
                the_shell_focus_setter(),
                the_input_target_listener(),
                the_shell_session_listener());
        });
}


std::shared_ptr<mf::Shell>
mir::DefaultServerConfiguration::the_frontend_shell()
{
    return the_session_manager();
}

std::shared_ptr<msh::FocusController>
mir::DefaultServerConfiguration::the_focus_controller()
{
    return the_session_manager();
}

std::initializer_list<std::shared_ptr<mi::EventFilter> const>
mir::DefaultServerConfiguration::the_event_filters()
{
    return empty_filter_list;
}

std::shared_ptr<mia::InputConfiguration>
mir::DefaultServerConfiguration::the_input_configuration()
{
    if (!input_configuration)
    {
        struct DefaultCursorListener : mi::CursorListener
        {
            DefaultCursorListener(std::weak_ptr<mg::Cursor> const& cursor) :
                cursor(cursor)
            {
            }

            void cursor_moved_to(float abs_x, float abs_y)
            {
                if (auto c = cursor.lock())
                {
                    c->move_to(geom::Point{geom::X(abs_x), geom::Y(abs_y)});
                }
            }

            std::weak_ptr<mg::Cursor> const cursor;
        };

        input_configuration = std::make_shared<mia::DefaultInputConfiguration>(
            the_event_filters(),
            the_display(),
            std::make_shared<DefaultCursorListener>(the_display()->the_cursor()));
    }
    return input_configuration;
}

std::shared_ptr<mi::InputManager>
mir::DefaultServerConfiguration::the_input_manager()
{
    return input_manager(
        [&, this]() -> std::shared_ptr<mi::InputManager>
        {
            if (the_options()->get("enable-input", enable_input_default))
            {
                if (the_options()->get(log_input, false))
                    ml::input_report::initialize(the_logger());
                return std::make_shared<mia::InputManager>(the_input_configuration());
            }
            else 
                return std::make_shared<mi::NullInputManager>();
        });
}

std::shared_ptr<mc::GraphicBufferAllocator>
mir::DefaultServerConfiguration::the_buffer_allocator()
{
    return buffer_allocator(
        [&]()
        {
            return the_graphics_platform()->create_buffer_allocator(the_buffer_initializer());
        });
}

std::shared_ptr<mg::Display>
mir::DefaultServerConfiguration::the_display()
{
    return display(
        [this]()
        {
            return the_graphics_platform()->create_display();
        });
}

std::shared_ptr<mg::ViewableArea> mir::DefaultServerConfiguration::the_viewable_area()
{
    return the_display();
}

std::shared_ptr<ms::SurfaceStackModel>
mir::DefaultServerConfiguration::the_surface_stack_model()
{
    return surface_stack(
        [this]()
        {
            return std::make_shared<ms::SurfaceStack>(the_buffer_bundle_factory());
        });
}

std::shared_ptr<mc::Renderables>
mir::DefaultServerConfiguration::the_renderables()
{
    return surface_stack(
        [this]()
        {
            return std::make_shared<ms::SurfaceStack>(the_buffer_bundle_factory());
        });
}

std::shared_ptr<msh::SurfaceFactory>
mir::DefaultServerConfiguration::the_shell_surface_factory()
{
    return shell_surface_factory(
        [this]()
        {
            auto surface_source = std::make_shared<msh::SurfaceSource>(
                the_surface_builder(),
                the_input_channel_factory());

            return std::make_shared<msh::OrganisingSurfaceFactory>(
                surface_source,
                the_shell_placement_strategy());
        });
}

std::shared_ptr<msh::SurfaceBuilder>
mir::DefaultServerConfiguration::the_surface_builder()
{
    return surface_controller(
        [this]()
        {
            return std::make_shared<ms::SurfaceController>(the_surface_stack_model());
        });
}

std::shared_ptr<mc::OverlayRenderer>
mir::DefaultServerConfiguration::the_overlay_renderer()
{
    struct NullOverlayRenderer : public mc::OverlayRenderer
    {
        virtual void render(mg::DisplayBuffer&) {}
    };
    return overlay_renderer(
        [this]()
        {
            return std::make_shared<NullOverlayRenderer>();
        });
}

std::shared_ptr<mc::CompositingStrategy>
mir::DefaultServerConfiguration::the_compositing_strategy()
{
    return compositing_strategy(
        [this]()
        {
            return std::make_shared<mc::DefaultCompositingStrategy>(the_renderables(), the_renderer(), the_overlay_renderer());
        });
}

std::shared_ptr<ms::BufferBundleFactory>
mir::DefaultServerConfiguration::the_buffer_bundle_factory()
{
    return buffer_bundle_manager(
        [this]()
        {
            return std::make_shared<mc::BufferBundleManager>(the_buffer_allocation_strategy());
        });
}

std::shared_ptr<mc::Compositor>
mir::DefaultServerConfiguration::the_compositor()
{
    return compositor(
        [this]()
        {
            return std::make_shared<mc::MultiThreadedCompositor>(the_display(),
                                                                 the_renderables(),
                                                                 the_compositing_strategy());
        });
}

std::shared_ptr<mir::frontend::ProtobufIpcFactory>
mir::DefaultServerConfiguration::the_ipc_factory(
    std::shared_ptr<mf::Shell> const& shell,
    std::shared_ptr<mg::ViewableArea> const& display,
    std::shared_ptr<mc::GraphicBufferAllocator> const& allocator)
{
    return ipc_factory(
        [&]()
        {
            return std::make_shared<DefaultIpcFactory>(
                shell,
                the_session_mediator_report(),
                the_message_processor_report(),
                the_graphics_platform(),
                display, allocator);
        });
}

std::shared_ptr<mf::SessionMediatorReport>
mir::DefaultServerConfiguration::the_session_mediator_report()
{
    return session_mediator_report(
        [this]() -> std::shared_ptr<mf::SessionMediatorReport>
        {
            if (the_options()->get(log_app_mediator, false))
            {
                return std::make_shared<ml::SessionMediatorReport>(the_logger());
            }
            else
            {
                return std::make_shared<mf::NullSessionMediatorReport>();
            }
        });
}

std::shared_ptr<mf::MessageProcessorReport>
mir::DefaultServerConfiguration::the_message_processor_report()
{
    return message_processor_report(
        [this]() -> std::shared_ptr<mf::MessageProcessorReport>
        {
            if (the_options()->get(log_msg_processor, false))
            {
                return std::make_shared<ml::MessageProcessorReport>(the_logger(), the_time_source());
            }
            else
            {
                return std::make_shared<mf::NullMessageProcessorReport>();
            }
        });
}


std::shared_ptr<ml::Logger> mir::DefaultServerConfiguration::the_logger()
{
    return logger(
        [this]() -> std::shared_ptr<ml::Logger>
        {
            if (the_options()->is_set(glog))
            {
                return std::make_shared<ml::GlogLogger>(
                    "mir",
                    the_options()->get(glog_stderrthreshold, 2),
                    the_options()->get(glog_minloglevel, 0),
                    the_options()->get(glog_log_dir, ""));
            }
            else
            {
                return std::make_shared<ml::DumbConsoleLogger>();
            }
        });
}

std::shared_ptr<mi::InputChannelFactory> mir::DefaultServerConfiguration::the_input_channel_factory()
{
    return the_input_manager();
}

std::shared_ptr<msh::InputTargetListener> mir::DefaultServerConfiguration::the_input_target_listener()
{
    return input_target_listener(
        [&]() -> std::shared_ptr<msh::InputTargetListener>
        {
            if (the_options()->get("enable-input", enable_input_default))
                return std::make_shared<mia::DispatcherController>(the_input_configuration());
            else
                return std::make_shared<mi::NullInputTargetListener>();
        });
}

std::shared_ptr<mir::time::TimeSource> mir::DefaultServerConfiguration::the_time_source()
{
    return time_source(
        []()
        {
            return std::make_shared<mir::time::HighResolutionClock>();
        });
}

std::shared_ptr<mir::MainLoop> mir::DefaultServerConfiguration::the_main_loop()
{
    return main_loop(
        []()
        {
            return std::make_shared<mir::AsioMainLoop>();
        });
}
