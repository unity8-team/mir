/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_TEST_DOUBLES_SHELL_H_
#define MIR_TEST_DOUBLES_SHELL_H_

#include "mir/scene/surface_creation_parameters.h"
#include "mir/frontend/shell.h"
#include "mir/frontend/surface_id.h"
#include "mir/scene/prompt_session_creation_parameters.h"

#include <gmock/gmock.h>

namespace mir
{
namespace test
{
namespace doubles
{

struct MockShell : public frontend::Shell
{
    MOCK_METHOD3(open_session, std::shared_ptr<frontend::Session>(
        pid_t client_pid,
        std::string const&,
        std::shared_ptr<frontend::EventSink> const&));

    MOCK_METHOD1(close_session, void(std::shared_ptr<frontend::Session> const&));

    MOCK_METHOD2(create_surface_for, frontend::SurfaceId(std::shared_ptr<frontend::Session> const&, scene::SurfaceCreationParameters const&));
    MOCK_METHOD1(handle_surface_created, void(std::shared_ptr<frontend::Session> const&));

    MOCK_METHOD2(start_prompt_session_for, std::shared_ptr<frontend::PromptSession>(
        std::shared_ptr<frontend::Session> const&,
        scene::PromptSessionCreationParameters const&));
    MOCK_METHOD2(add_prompt_provider_for, void(
        std::shared_ptr<frontend::PromptSession> const&,
        std::shared_ptr<frontend::Session> const&));
    MOCK_METHOD1(stop_prompt_session, void(std::shared_ptr<frontend::PromptSession> const&));
};

}
}
} // namespace mir

#endif // MIR_TEST_DOUBLES_SHELL_H_
