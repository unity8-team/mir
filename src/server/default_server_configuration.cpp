/*
 * Copyright © 2012-2014 Canonical Ltd.
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
#include "mir/fatal.h"
#include "mir/options/default_configuration.h"
#include "mir/abnormal_exit.h"
#include "mir/asio_main_loop.h"
#include "mir/default_server_status_listener.h"
#include "mir/emergency_cleanup.h"
#include "mir/default_configuration.h"

#include "mir/options/program_option.h"
#include "mir/frontend/session_credentials.h"
#include "mir/frontend/session_authorizer.h"
#include "mir/scene/surface_configurator.h"
#include "mir/graphics/cursor.h"
#include "mir/scene/null_session_listener.h"
#include "mir/graphics/display.h"
#include "mir/input/cursor_listener.h"
#include "mir/input/vt_filter.h"
#include "mir/input/input_manager.h"
#include "mir/time/high_resolution_clock.h"
#include "mir/geometry/rectangles.h"
#include "mir/default_configuration.h"
#include "mir/scene/null_prompt_session_listener.h"
#include "default_emergency_cleanup.h"

#include <unordered_map>

namespace mc = mir::compositor;
namespace geom = mir::geometry;
namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mo = mir::options;
namespace ms = mir::scene;
namespace msh = mir::shell;
namespace mi = mir::input;

namespace
{

struct hash_type_info
{
    int operator()(std::type_info const* info) const noexcept
    {
        return info->hash_code();
    }
};
struct CachedVoidPtr
{
    std::function<std::shared_ptr<void>()> creator;
    std::weak_ptr<void> cache;
    CachedVoidPtr(std::function<std::shared_ptr<void>()> creator)
        : creator(creator)
    {}
    std::shared_ptr<void> operator()()
    {
        auto cached_object = cache.lock();
        if (cached_object)
            return cached_object;
        cache = cached_object = creator();
        return cached_object;
    }
};

}

struct mir::DefaultServerConfiguration::PrivateImplementation
{
    std::unordered_map<std::type_info const*,std::shared_ptr<CachedVoidPtr>,hash_type_info> cached_objects;

    // store constructors separately forward to real constructor
};

mir::DefaultServerConfiguration::DefaultServerConfiguration(int argc, char const* argv[]) :
        DefaultServerConfiguration(std::make_shared<mo::DefaultConfiguration>(argc, argv))
{
}

mir::DefaultServerConfiguration::DefaultServerConfiguration(std::shared_ptr<mo::Configuration> const& configuration_options) :
    pimpl(new PrivateImplementation),
    configuration_options(configuration_options),
    default_filter(std::make_shared<mi::VTFilter>())
{
}

mir::DefaultServerConfiguration::~DefaultServerConfiguration()
{}


void mir::DefaultServerConfiguration::store_constructor(std::function<std::shared_ptr<void>()> const&& constructor, std::type_info const& interface)
{
    pimpl->cached_objects[&interface] = std::make_shared<CachedVoidPtr>(constructor);
}

void mir::DefaultServerConfiguration::wrap_existing_interface(std::function<std::shared_ptr<void>(std::shared_ptr<void>)> const&& cast_function, std::type_info const& base_interface, std::type_info const& interface)
{
    auto it = pimpl->cached_objects.find(&base_interface);
    if (it == pimpl->cached_objects.end())
        throw std::logic_error("base interface does not exist");

    auto base_cache = it->second;
    store_constructor(
        [base_cache,cast_function]
        {
            return cast_function((*base_cache)());
        },
        interface
        );
}

std::shared_ptr<void> mir::DefaultServerConfiguration::get(std::type_info const& interface)
{
    auto it = pimpl->cached_objects.find(&interface);
    if (it == pimpl->cached_objects.end())
        throw std::logic_error("interface does not exist");
    return (*it->second)();
}

auto mir::DefaultServerConfiguration::the_options() const
->std::shared_ptr<options::Option>
{
    return configuration_options->the_options();
}

std::string mir::DefaultServerConfiguration::the_socket_file() const
{
    auto socket_file = the_options()->get<std::string>(options::server_socket_opt);

    // Record this for any children that want to know how to connect to us.
    // By both listening to this env var on startup and resetting it here,
    // we make it easier to nest Mir servers.
    setenv("MIR_SOCKET", socket_file.c_str(), 1);

    return socket_file;
}

std::shared_ptr<ms::SessionListener>
mir::DefaultServerConfiguration::the_session_listener()
{
    return session_listener(
        [this]
        {
            return std::make_shared<ms::NullSessionListener>();
        });
}

std::shared_ptr<ms::PromptSessionListener>
mir::DefaultServerConfiguration::the_prompt_session_listener()
{
    return prompt_session_listener(
        [this]
        {
            return std::make_shared<ms::NullPromptSessionListener>();
        });
}

std::shared_ptr<ms::SurfaceConfigurator> mir::DefaultServerConfiguration::the_surface_configurator()
{
    struct DefaultSurfaceConfigurator : public ms::SurfaceConfigurator
    {
        int select_attribute_value(ms::Surface const&, MirSurfaceAttrib, int requested_value)
        {
            return requested_value;
        }
        void attribute_set(ms::Surface const&, MirSurfaceAttrib, int)
        {
        }
    };
    return surface_configurator(
        [this]()
        {
            return std::make_shared<DefaultSurfaceConfigurator>();
        });
}

std::shared_ptr<mf::SessionAuthorizer>
mir::DefaultServerConfiguration::the_session_authorizer()
{
    struct DefaultSessionAuthorizer : public mf::SessionAuthorizer
    {
        bool connection_is_allowed(mf::SessionCredentials const& /* creds */)
        {
            return true;
        }

        bool configure_display_is_allowed(mf::SessionCredentials const& /* creds */)
        {
            return true;
        }

        bool screencast_is_allowed(mf::SessionCredentials const& /* creds */)
        {
            return true;
        }

        bool prompt_session_is_allowed(mf::SessionCredentials const& /* creds */) override
        {
            return true;
        }
    };
    return session_authorizer(
        [&]()
        {
            return std::make_shared<DefaultSessionAuthorizer>();
        });
}

mir::CachedPtr<mir::time::Clock> mir::DefaultServerConfiguration::clock;

std::shared_ptr<mir::time::Clock> mir::DefaultServerConfiguration::the_clock()
{
    return clock(
        []()
        {
            return std::make_shared<mir::time::HighResolutionClock>();
        });
}

std::shared_ptr<mir::MainLoop> mir::DefaultServerConfiguration::the_main_loop()
{
    return main_loop(
        [this]()
        {
            return std::make_shared<mir::AsioMainLoop>(the_clock());
        });
}

std::shared_ptr<mir::ServerActionQueue> mir::DefaultServerConfiguration::the_server_action_queue()
{
    return the_main_loop();
}

std::shared_ptr<mir::ServerStatusListener> mir::DefaultServerConfiguration::the_server_status_listener()
{
    return server_status_listener(
        []()
        {
            return std::make_shared<mir::DefaultServerStatusListener>();
        });
}

std::shared_ptr<mir::EmergencyCleanup> mir::DefaultServerConfiguration::the_emergency_cleanup()
{
    return emergency_cleanup(
        []()
        {
            return std::make_shared<DefaultEmergencyCleanup>();
        });
}

auto mir::DefaultServerConfiguration::the_fatal_error_strategy()
-> void (*)(char const* reason, ...)
{
    if (the_options()->is_set(options::fatal_abort_opt))
        return &fatal_error_abort;
    else
        return fatal_error;
}
