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

#ifndef PROCESS_CONTROLLER_H
#define PROCESS_CONTROLLER_H

#include <QSharedPointer>

#include <sys/types.h>

namespace qtmir
{

class ProcessController
{
public:
    class OomController
    {
    public:
        OomController();
        OomController(const OomController&) = delete;
        virtual ~OomController() = default;

        OomController& operator=(const OomController&) = delete;

        virtual void ensureProcessLikelyToBeKilled(pid_t);
        virtual void ensureProcessLessLikelyToBeKilled(pid_t);
        virtual void ensureProcessUnlikelyToBeKilled(pid_t);
    };

    ProcessController();
    ProcessController(const ProcessController&) = delete;
    virtual ~ProcessController();

    ProcessController& operator=(const ProcessController&) = delete;

    virtual const QSharedPointer<OomController>& oomController() const;

    virtual bool sigStopProcessGroupForPid(pid_t pid) const;
    virtual bool sigContinueProcessGroupForPid(pid_t pid) const;

protected:
    QSharedPointer<OomController> m_oomController;
};

} // namespace qtmir

#endif // PROCESS_CONTROLLER_H
