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
 * Authored by: Kevin DuBois <kevin.dubois@canonical.com>
 */

#include <condition_variable>
#include <boost/throw_exception.hpp>
#include "mediating_display_changer.h"
#include "session_container.h"
#include "mir/scene/session.h"
#include "session_event_handler_register.h"
#include "mir/graphics/display.h"
#include "mir/compositor/compositor.h"
#include "mir/geometry/rectangles.h"
#include "mir/graphics/display_configuration_policy.h"
#include "mir/graphics/display_configuration.h"
#include "mir/graphics/display_configuration_report.h"
#include "mir/server_action_queue.h"
#include "mir/time/alarm_factory.h"
#include "mir/time/alarm.h"

namespace mf = mir::frontend;
namespace ms = mir::scene;
namespace mg = mir::graphics;
namespace mc = mir::compositor;
namespace mi = mir::input;
namespace mt = mir::time;

namespace
{

class ApplyNowAndRevertOnScopeExit
{
public:
    ApplyNowAndRevertOnScopeExit(std::function<void()> const& apply,
                                 std::function<void()> const& revert)
        : revert{revert}
    {
        apply();
    }

    ~ApplyNowAndRevertOnScopeExit()
    {
        revert();
    }

private:
    ApplyNowAndRevertOnScopeExit(ApplyNowAndRevertOnScopeExit const&) = delete;
    ApplyNowAndRevertOnScopeExit& operator=(ApplyNowAndRevertOnScopeExit const&) = delete;

    std::function<void()> const revert;
};
}

ms::MediatingDisplayChanger::MediatingDisplayChanger(
    std::shared_ptr<mg::Display> const& display,
    std::shared_ptr<mc::Compositor> const& compositor,
    std::shared_ptr<mg::DisplayConfigurationPolicy> const& display_configuration_policy,
    std::shared_ptr<SessionContainer> const& session_container,
    std::shared_ptr<SessionEventHandlerRegister> const& session_event_handler_register,
    std::shared_ptr<ServerActionQueue> const& server_action_queue,
    std::shared_ptr<mg::DisplayConfigurationReport> const& report,
    std::shared_ptr<mi::InputRegion> const& region,
    std::shared_ptr<mt::AlarmFactory> const& alarm_factory)
    : display{display},
      compositor{compositor},
      display_configuration_policy{display_configuration_policy},
      session_container{session_container},
      session_event_handler_register{session_event_handler_register},
      server_action_queue{server_action_queue},
      report{report},
      base_configuration_{display->configuration()},
      base_configuration_applied{true},
      region{region},
      alarm_factory{alarm_factory}
{
    session_event_handler_register->register_focus_change_handler(
        [this](std::shared_ptr<ms::Session> const& session)
        {
            auto const weak_session = std::weak_ptr<ms::Session>(session);
            this->server_action_queue->enqueue(
                this,
                [this,weak_session]
                {
                    if (auto const session = weak_session.lock())
                        focus_change_handler(session);
                });
        });

    session_event_handler_register->register_no_focus_handler(
        [this]
        {
            this->server_action_queue->enqueue(
                this,
                [this] { no_focus_handler(); });
        });

    session_event_handler_register->register_session_stopping_handler(
        [this](std::shared_ptr<ms::Session> const& session)
        {
            auto const weak_session = std::weak_ptr<ms::Session>(session);
            this->server_action_queue->enqueue(
                this,
                [this,weak_session]
                {
                    if (auto const session = weak_session.lock())
                        session_stopping_handler(session);
                });
        });

    report->initial_configuration(*base_configuration_);
    update_input_rectangles(*base_configuration_);
}

void ms::MediatingDisplayChanger::configure(
    std::shared_ptr<mf::Session> const& session,
    std::shared_ptr<mg::DisplayConfiguration> const& conf)
{
    {
        std::lock_guard<std::mutex> lg{configuration_mutex};
        config_map[session] = conf;

        if (session != focused_session.lock())
            return;
    }

    std::weak_ptr<mf::Session> const weak_session{session};

    server_action_queue->enqueue(
        this,
        [this, weak_session, conf]
        {
            if (auto const session = weak_session.lock())
            {
                std::lock_guard<std::mutex> lg{configuration_mutex};

                /* If the session is focused, apply the configuration */
                if (focused_session.lock() == session)
                    apply_config(conf, PauseResumeSystem);
            }
        });
}

void
ms::MediatingDisplayChanger::preview_base_configuration(
    std::weak_ptr<frontend::Session> const& session,
    std::shared_ptr<graphics::DisplayConfiguration> const& conf,
    std::chrono::seconds timeout)
{
    {
        std::lock_guard<std::mutex> lock{configuration_mutex};

        if (preview_configuration_timeout)
        {
            BOOST_THROW_EXCEPTION(
                std::runtime_error{"Another client is currently changing base configuration"});
        }

        preview_configuration_timeout = alarm_factory->create_alarm(
            [this, session]()
                {
                    if (auto live_session = session.lock())
                    {
                        apply_base_config(PauseResumeSystem);
                        live_session->send_display_config(*base_configuration());
                    }
                });
    }

    server_action_queue->enqueue(
        this,
        [this, conf, session, timeout]()
        {
            if (auto live_session = session.lock())
            {
                {
                    std::lock_guard<std::mutex> lock{configuration_mutex};
                    preview_configuration_timeout->reschedule_in(timeout);
                }

                apply_config(conf, PauseResumeSystem);
                live_session->send_display_config(*conf);
            }
        });
}

void
ms::MediatingDisplayChanger::confirm_base_configuration(
    std::shared_ptr<frontend::Session> const& /*session*/,
    std::shared_ptr<graphics::DisplayConfiguration> const& confirmed_conf)
{
    {
        std::lock_guard<std::mutex> lock{configuration_mutex};
        preview_configuration_timeout = std::unique_ptr<mt::Alarm>();
    }
    set_base_configuration(confirmed_conf);
}

std::shared_ptr<mg::DisplayConfiguration>
ms::MediatingDisplayChanger::base_configuration()
{
    std::lock_guard<std::mutex> lg{configuration_mutex};

    return base_configuration_->clone();
}

void ms::MediatingDisplayChanger::configure_for_hardware_change(
    std::shared_ptr<graphics::DisplayConfiguration> const& conf,
    SystemStateHandling pause_resume_system)
{
    server_action_queue->enqueue(
        this,
        [this, conf, pause_resume_system]
        {
            std::lock_guard<std::mutex> lg{configuration_mutex};

            display_configuration_policy->apply_to(*conf);
            base_configuration_ = conf;
            if (base_configuration_applied)
                apply_base_config(pause_resume_system);

            /*
             * Clear all the per-session configurations, since they may have become
             * invalid due to the hardware change.
             */
            config_map.clear();

            /* Send the new configuration to all the sessions */
            send_config_to_all_sessions(conf);
        });
}

void ms::MediatingDisplayChanger::pause_display_config_processing()
{
    server_action_queue->pause_processing_for(this);
}

void ms::MediatingDisplayChanger::resume_display_config_processing()
{
    server_action_queue->resume_processing_for(this);
}

void ms::MediatingDisplayChanger::apply_config(
    std::shared_ptr<graphics::DisplayConfiguration> const& conf,
    SystemStateHandling pause_resume_system)
{
    report->new_configuration(*conf);
    if (pause_resume_system)
    {
        ApplyNowAndRevertOnScopeExit comp{
            [this] { compositor->stop(); },
            [this] { compositor->start(); }};

        display->configure(*conf);
    }
    else
    {
        display->configure(*conf);
    }
    update_input_rectangles(*conf);

    base_configuration_applied = false;
}

void ms::MediatingDisplayChanger::apply_base_config(
    SystemStateHandling pause_resume_system)
{
    apply_config(base_configuration_, pause_resume_system);
    base_configuration_applied = true;
}

void ms::MediatingDisplayChanger::send_config_to_all_sessions(
    std::shared_ptr<mg::DisplayConfiguration> const& conf)
{
    session_container->for_each(
        [&conf](std::shared_ptr<Session> const& session)
        {
            session->send_display_config(*conf);
        });
}

void ms::MediatingDisplayChanger::focus_change_handler(
    std::shared_ptr<ms::Session> const& session)
{
    std::lock_guard<std::mutex> lg{configuration_mutex};

    focused_session = session;

    /*
     * If the newly focused session has a display configuration, apply it.
     * Otherwise if we aren't currently using the base configuration,
     * apply that.
     */
    auto const it = config_map.find(session);
    if (it != config_map.end())
    {
        apply_config(it->second, PauseResumeSystem);
    }
    else if (!base_configuration_applied)
    {
        apply_base_config(PauseResumeSystem);
    }
}

void ms::MediatingDisplayChanger::no_focus_handler()
{
    std::lock_guard<std::mutex> lg{configuration_mutex};

    focused_session.reset();
    if (!base_configuration_applied)
    {
        apply_base_config(PauseResumeSystem);
    }
}

void ms::MediatingDisplayChanger::session_stopping_handler(
    std::shared_ptr<ms::Session> const& session)
{
    std::lock_guard<std::mutex> lg{configuration_mutex};

    config_map.erase(session);
}

void ms::MediatingDisplayChanger::set_base_configuration(std::shared_ptr<mg::DisplayConfiguration> const &conf)
{
    server_action_queue->enqueue(
        this,
        [this, conf]
        {
            std::lock_guard<std::mutex> lg{configuration_mutex};

            base_configuration_ = conf;
            if (base_configuration_applied)
                apply_base_config(PauseResumeSystem);

            send_config_to_all_sessions(conf);
        });
}

void ms::MediatingDisplayChanger::update_input_rectangles(mg::DisplayConfiguration const& config)
{
    geometry::Rectangles rectangles;
    config.for_each_output([&rectangles](mg::DisplayConfigurationOutput const& output) {
        if (output.used && output.connected && output.power_mode == mir_power_mode_on &&
            output.current_mode_index < output.modes.size())
            rectangles.add(geometry::Rectangle(output.top_left, output.modes[output.current_mode_index].size));
    });
    region->set_input_rectangles(rectangles);
}
