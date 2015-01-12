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
 * Authored By: Nick Dedekind <nick.dedekind@canonical.com>
 */

#ifndef MIR_SCENE_PROMPT_SESSION_LISTENER_H_
#define MIR_SCENE_PROMPT_SESSION_LISTENER_H_

#include <memory>

namespace mir
{
namespace scene
{
class Session;
class PromptSession;

class PromptSessionListener
{
public:
    virtual void starting(std::shared_ptr<PromptSession> const& prompt_session) = 0;
    virtual void stopping(std::shared_ptr<PromptSession> const& prompt_session) = 0;
    virtual void suspending(std::shared_ptr<PromptSession> const& prompt_session) = 0;
    virtual void resuming(std::shared_ptr<PromptSession> const& prompt_session) = 0;

    virtual void prompt_provider_added(PromptSession const& prompt_session, std::shared_ptr<Session> const& prompt_provider) = 0;
    virtual void prompt_provider_removed(PromptSession const& prompt_session, std::shared_ptr<Session> const& prompt_provider) = 0;

protected:
    PromptSessionListener() = default;
    virtual ~PromptSessionListener() = default;

    PromptSessionListener(const PromptSessionListener&) = delete;
    PromptSessionListener& operator=(const PromptSessionListener&) = delete;
};

}
}


#endif // MIR_SCENE_PROMPT_SESSION_LISTENER_H_
