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

#ifndef MOCK_MIR_SHELL_FOCUS_CONTROLLER_H
#define MOCK_MIR_SHELL_FOCUS_CONTROLLER_H

#include <mir/shell/focus_controller.h>
#include <gmock/gmock.h>

#include <string>

namespace mir {
namespace shell {

class MockFocusController : public FocusController
{
public:
    MOCK_METHOD0(focus_next, void());
    MOCK_CONST_METHOD0(focussed_application, std::weak_ptr<scene::Session>());
    MOCK_METHOD1(set_focus_to, void(std::shared_ptr<scene::Session>const&));
};

} // namespace shell
} // namespace mir

#endif // MOCK_MIR_SHELL_FOCUS_CONTROLLER_H_
