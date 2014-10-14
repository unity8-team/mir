/*
 * Copyright (C) 2013 Canonical, Ltd.
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
#ifndef MOCK_OOM_CONTROLLER_H
#define MOCK_OOM_CONTROLLER_H

#include <Unity/Application/processcontroller.h>

#include <gmock/gmock.h>

namespace testing
{
struct MockOomController : public qtmir::ProcessController::OomController
{
    MOCK_METHOD1(ensureProcessLikelyToBeKilled, void(pid_t));
    MOCK_METHOD1(ensureProcessLessLikelyToBeKilled, void(pid_t));
    MOCK_METHOD1(ensureProcessUnlikelyToBeKilled, void(pid_t));
};
}

#endif // MOCK_OOM_CONTROLLER_H
