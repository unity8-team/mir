/*
 * Copyright (C) 2014 Canonical, Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License version 3, as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranties of MERCHANTABILITY,
 * SATISFACTORY QUALITY, or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MOCK_MIR_PROMPT_SESSION_H
#define MOCK_MIR_PROMPT_SESSION_H

#include <mir/scene/prompt_session.h>
#include <gmock/gmock.h>

namespace mir {
namespace scene {

struct MockPromptSession : public PromptSession
{
public:
    MOCK_METHOD1(start, void(std::shared_ptr<Session> const&));
    MOCK_METHOD1(stop, void(std::shared_ptr<Session> const&));
    MOCK_METHOD1(suspend, void(std::shared_ptr<Session> const&));
    MOCK_METHOD1(resume, void(std::shared_ptr<Session> const&));
};

} // namespace scene
} // namespace mir

#endif // MOCK_MIR_PROMPT_SESSION_H
