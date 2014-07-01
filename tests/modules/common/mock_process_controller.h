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
#ifndef MOCK_PROCESS_CONTROLLER_H
#define MOCK_PROCESS_CONTROLLER_H

#include <Unity/Application/processcontroller.h>

#include <gmock/gmock.h>

namespace testing
{
struct MockProcessController : public qtmir::ProcessController
{
    MockProcessController(const QSharedPointer<qtmir::ProcessController::OomController>& oomController)
        : m_oomController(oomController)
    {
    }

    const QSharedPointer<qtmir::ProcessController::OomController>& oomController() const
    {
        return m_oomController;
    }

    MOCK_CONST_METHOD1(sigStopProcessGroupForPid, bool(pid_t));
    MOCK_CONST_METHOD1(sigContinueProcessGroupForPid, bool(pid_t));

    QSharedPointer<qtmir::ProcessController::OomController> m_oomController;
};
}

#endif // MOCK_PROCESS_CONTROLLER_H
