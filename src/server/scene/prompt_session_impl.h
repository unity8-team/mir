/*
 * Copyright © 2014 Canonical Ltd.
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
 * Authored by: Nick Dedekind <nick.dedekind@canonical.com>
 */

#ifndef MIR_SCENE_PROMPT_SESSION_IMPL_H_
#define MIR_SCENE_PROMPT_SESSION_IMPL_H_

#include "mir/scene/prompt_session.h"

#include <mutex>

namespace mir
{
namespace scene
{

class PromptSessionImpl : public scene::PromptSession
{
public:
    explicit PromptSessionImpl();

    void start(std::shared_ptr<Session> const& helper_session) override;
    void stop(std::shared_ptr<Session> const& helper_session) override;
    void suspend(std::shared_ptr<Session> const& helper_session) override;
    void resume(std::shared_ptr<Session> const& helper_session) override;

    MirPromptSessionState state() const;

private:
    std::mutex mutable guard;
    MirPromptSessionState current_state;
};
}
}

#endif // MIR_SCENE_PROMPT_SESSION_IMPL_H_
