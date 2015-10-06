/*
 * Copyright © 2015 Canonical Ltd.
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
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SHELL_FRONTEND_SHELL_H_
#define MIR_SHELL_FRONTEND_SHELL_H_

#include "mir/frontend/shell.h"

namespace ms = mir::scene;
namespace mf = mir::frontend;

namespace mir
{
namespace shell
{
class Shell;
class PersistentSurfaceStore;

namespace detail
{
// Adapter class to translate types between frontend and shell
struct FrontendShell : mf::Shell
{
    std::shared_ptr<shell::Shell> const wrapped;
    std::shared_ptr<shell::PersistentSurfaceStore> const surface_store;

    explicit FrontendShell(std::shared_ptr<shell::Shell> const& wrapped,
                           std::shared_ptr<shell::PersistentSurfaceStore> const& surface_store)
        : wrapped{wrapped},
          surface_store{surface_store}
    {
    }

    std::shared_ptr<mf::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<mf::EventSink> const& sink) override;

    void close_session(std::shared_ptr<mf::Session> const& session) override;

    std::shared_ptr<mf::PromptSession> start_prompt_session_for(
        std::shared_ptr<mf::Session> const& session,
        ms::PromptSessionCreationParameters const& params) override;

    void add_prompt_provider_for(
        std::shared_ptr<mf::PromptSession> const& prompt_session,
        std::shared_ptr<mf::Session> const& session) override;

    void stop_prompt_session(std::shared_ptr<mf::PromptSession> const& prompt_session) override;

    mf::SurfaceId create_surface(
        std::shared_ptr<mf::Session> const& session,
        ms::SurfaceCreationParameters const& params,
        std::shared_ptr<mf::EventSink> const& sink) override;

    void modify_surface(std::shared_ptr<mf::Session> const& session, mf::SurfaceId surface, SurfaceSpecification const& modifications) override;

    void destroy_surface(std::shared_ptr<mf::Session> const& session, mf::SurfaceId surface) override;

    std::string persistent_id_for(std::shared_ptr<mf::Session> const& session, mf::SurfaceId surface) override;

    std::shared_ptr<ms::Surface> surface_for_id(std::string const& serialized_id) override;

    int set_surface_attribute(
        std::shared_ptr<mf::Session> const& session,
        mf::SurfaceId surface_id,
        MirSurfaceAttrib attrib,
        int value) override;

    int get_surface_attribute(
        std::shared_ptr<mf::Session> const& session,
        mf::SurfaceId surface_id,
        MirSurfaceAttrib attrib) override;
};
}
}
}

#endif /* MIR_SHELL_FRONTEND_SHELL_H_ */
