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


#ifndef MIR_FRONTEND_SESSION_MEDIATOR_REPORT_H_
#define MIR_FRONTEND_SESSION_MEDIATOR_REPORT_H_

#include <string>

#include <sys/types.h>

namespace mir
{
namespace frontend
{
// Interface for monitoring application activity
class SessionMediatorReport
{
public:
    virtual ~SessionMediatorReport() { /* TODO: make nothrow */ }

    virtual void session_connect_called(std::string const& app_name) = 0;

    virtual void session_create_surface_called(std::string const& app_name) = 0;

    virtual void session_next_buffer_called(std::string const& app_name) = 0;

    virtual void session_exchange_buffer_called(std::string const& app_name) = 0;

    virtual void session_release_surface_called(std::string const& app_name) = 0;

    virtual void session_disconnect_called(std::string const& app_name) = 0;

    virtual void session_drm_auth_magic_called(std::string const& app_name) = 0;

    virtual void session_configure_surface_called(std::string const& app_name) = 0;

    virtual void session_configure_surface_cursor_called(std::string const& app_name) = 0;

    virtual void session_configure_display_called(std::string const& app_name) = 0;

    virtual void session_start_prompt_session_called(std::string const& app_name, pid_t application_process) = 0;

    virtual void session_stop_prompt_session_called(std::string const& app_name) = 0;

    virtual void session_error(
        std::string const& app_name,
        char const* method,
        std::string const& what) = 0;
};

}
}


#endif /* MIR_FRONTEND_SESSION_MEDIATOR_REPORT_H_ */
