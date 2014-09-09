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
 * Authored by:
 *   Alan Griffiths <alan@octopull.co.uk>
 *   Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/display_server.h"
#include "mir/server_configuration.h"
#include "mir/main_loop.h"
#include "mir/server_status_listener.h"
#include "mir/display_changer.h"

#include "mir/compositor/compositor.h"
#include "mir/frontend/connector.h"
#include "mir/graphics/display.h"
#include "mir/graphics/display_configuration.h"
#include "mir/input/input_manager.h"
#include "mir/input/input_dispatcher.h"

#include <stdexcept>

namespace mc = mir::compositor;
namespace mf = mir::frontend;
namespace mg = mir::graphics;
namespace mi = mir::input;
namespace msh = mir::shell;

namespace
{

class TryButRevertIfUnwinding
{
public:
    TryButRevertIfUnwinding(std::function<void()> const& apply,
                            std::function<void()> const& revert)
        : revert{revert}
    {
        apply();
    }

    ~TryButRevertIfUnwinding()
    {
        if (std::uncaught_exception())
            revert();
    }

private:
    TryButRevertIfUnwinding(TryButRevertIfUnwinding const&) = delete;
    TryButRevertIfUnwinding& operator=(TryButRevertIfUnwinding const&) = delete;

    std::function<void()> const revert;
};

}

struct mir::DisplayServer::Private
{
    Private(ServerConfiguration& config)
        : emergency_cleanup{config.the_emergency_cleanup()},
          graphics_platform{config.the_graphics_platform()},
          display{config.the_display()},
          input_dispatcher{config.the_input_dispatcher()},
          input_configuration{config.the_input_configuration()},
          compositor{config.the_compositor()},
          connector{config.the_connector()},
          prompt_connector{config.the_prompt_connector()},
          input_manager{config.the_input_manager()},
          main_loop{config.the_main_loop()},
          server_status_listener{config.the_server_status_listener()},
          display_changer{config.the_display_changer()}
    {
        display->register_configuration_change_handler(
            *main_loop,
            [this] { return configure_display(); });

        display->register_pause_resume_handlers(
            *main_loop,
            [this] { return pause(); },
            [this] { return resume(); });
    }

    bool pause()
    {
        try
        {
            TryButRevertIfUnwinding dispatcher{
                [this] { input_dispatcher->stop(); },
                [this] { input_dispatcher->start(); }};

            TryButRevertIfUnwinding input{
                [this] { input_manager->stop(); },
                [this] { input_manager->start(); }};

            TryButRevertIfUnwinding display_config_processing{
                [this] { display_changer->pause_display_config_processing(); },
                [this] { display_changer->resume_display_config_processing(); }};

            TryButRevertIfUnwinding comp{
                [this] { compositor->stop(); },
                [this] { compositor->start(); }};

            TryButRevertIfUnwinding prompt{
                [this] { prompt_connector->stop(); },
                [this] { prompt_connector->start(); }};

            TryButRevertIfUnwinding comm{
                [this] { connector->stop(); },
                [this] { connector->start(); }};

            display->pause();
        }
        catch(std::runtime_error const&)
        {
            return false;
        }

        server_status_listener->paused();

        return true;
    }

    bool resume()
    {
        try
        {
            TryButRevertIfUnwinding disp{
                [this] { display->resume(); },
                [this] { display->pause(); }};

            TryButRevertIfUnwinding comm{
                [this] { connector->start(); },
                [this] { connector->stop(); }};

            TryButRevertIfUnwinding prompt{
                [this] { prompt_connector->start(); },
                [this] { prompt_connector->stop(); }};

            TryButRevertIfUnwinding display_config_processing{
                [this] { display_changer->resume_display_config_processing(); },
                [this] { display_changer->pause_display_config_processing(); }};

            TryButRevertIfUnwinding input{
                [this] { input_manager->start(); },
                [this] { input_manager->stop(); }};

            TryButRevertIfUnwinding dispatcher{
                [this] { input_dispatcher->start(); },
                [this] { input_dispatcher->stop(); }};

            compositor->start();
        }
        catch(std::runtime_error const&)
        {
            return false;
        }

        server_status_listener->resumed();

        return true;
    }

    void configure_display()
    {
        std::shared_ptr<graphics::DisplayConfiguration> conf =
            display->configuration();

        display_changer->configure_for_hardware_change(
            conf, DisplayChanger::PauseResumeSystem);
    }

    std::shared_ptr<EmergencyCleanup> const emergency_cleanup; // Hold this so it does not get freed prematurely
    std::shared_ptr<mg::Platform> const graphics_platform; // Hold this so the platform is loaded once
    std::shared_ptr<mg::Display> const display;
    std::shared_ptr<mi::InputDispatcher> const input_dispatcher;
    std::shared_ptr<input::InputConfiguration> const input_configuration;
    std::shared_ptr<mc::Compositor> const compositor;
    std::shared_ptr<mf::Connector> const connector;
    std::shared_ptr<mf::Connector> const prompt_connector;
    std::shared_ptr<mi::InputManager> const input_manager;
    std::shared_ptr<mir::MainLoop> const main_loop;
    std::shared_ptr<mir::ServerStatusListener> const server_status_listener;
    std::shared_ptr<mir::DisplayChanger> const display_changer;
};

mir::DisplayServer::DisplayServer(ServerConfiguration& config) :
    p(new DisplayServer::Private{config})
{
}

/*
 * Need to define the destructor in the source file, so that we
 * can define the 'p' member variable as a unique_ptr to an
 * incomplete type (DisplayServerPrivate) in the header.
 */
mir::DisplayServer::~DisplayServer() = default;

void mir::DisplayServer::run()
{
    p->connector->start();
    p->prompt_connector->start();
    p->compositor->start();
    p->input_manager->start();
    p->input_dispatcher->start();

    p->server_status_listener->started();

    p->main_loop->run();

    p->input_dispatcher->stop();
    p->input_manager->stop();
    p->compositor->stop();
    p->prompt_connector->stop();
    p->connector->stop();
}

void mir::DisplayServer::stop()
{
    p->main_loop->stop();
}
