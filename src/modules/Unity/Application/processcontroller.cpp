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

#include "processcontroller.h"

#include "logging.h"

// Process C++
#include <core/posix/process.h>
#include <core/posix/this_process.h>
#include <core/posix/linux/proc/process/oom_adj.h>
#include <core/posix/linux/proc/process/oom_score_adj.h>

// std
#include <signal.h>
#include <unistd.h>

namespace plpp = core::posix::linux::proc::process;

namespace qtmir
{

ProcessController::OomController::OomController()
{
}

void ProcessController::OomController::ensureProcessLikelyToBeKilled(pid_t pid)
{
    // We avoid boundary values for oom_score_adj. For that, we
    // set it to 80% of the total available range.
    static const float defaultPercentage = 0.8;

    core::posix::Process process(pid);

    try {
        plpp::OomScoreAdj shellScore;
        core::posix::this_process::instance() >> shellScore;

        plpp::OomScoreAdj processScore
        {
            static_cast<int>((plpp::OomScoreAdj::max_value() - shellScore.value) * defaultPercentage) + shellScore.value
        };

        process << processScore;
    } catch(...) {
        // Accessing OomScoreAdj resulted in an exception being thrown.
        // Trying with the deprecated OomAdj now as a last resort.
        try
        {
            process << plpp::OomAdj{plpp::OomAdj::max_value()};
        } catch(...)
        {
            qDebug() << "ensureProcessIsLikelyToBeKilled failed for pid=" << pid;
        }
    }
}

void ProcessController::OomController::ensureProcessUnlikelyToBeKilled(pid_t pid)
{
    // By system default, we set the oom_score_adj of Unity8 to -10 (via lightdm).
    // As we want to avoid that any app's oom_score_adj is <= Unity8's oom_score_adj,
    // we choose a default increase of +1.
    static const int default_increase = 1;

    core::posix::Process process(pid);

    try {
        plpp::OomScoreAdj shellScore;
        core::posix::this_process::instance() >> shellScore;

        plpp::OomScoreAdj processScore
        {
            shellScore.value + default_increase
        };

        process << processScore;
    } catch(...) {
        // Accessing OomScoreAdj resulted in an exception being thrown.
        // Trying with the deprecated OomAdj now as a last resort.
        // By system default, we set the oom_score_adj of Unity8 to -10 (via lightdm).
        // As we want to avoid that any app's oom_score_adj or oom_adj is <= Unity8's oom_score_adj,
        // we choose a default value of -9 for oom_score_adj and 0 for oom_adj.
        static const int defaultValue = 0;

        try
        {
            process << plpp::OomAdj{defaultValue};
        } catch(...)
        {
            qDebug() << "ensureProcessIsUnlikelyToBeKilled failed for pid" << pid;
        }
    }
}

ProcessController::ProcessController()
    : m_oomController(new ProcessController::OomController())
{
}

ProcessController::~ProcessController()
{
}

const QSharedPointer<ProcessController::OomController>& ProcessController::oomController() const
{
    return m_oomController;
}

bool ProcessController::sigStopProcessGroupForPid(pid_t pid) const
{
    return -1 != kill(-pid, SIGSTOP);
}

bool ProcessController::sigContinueProcessGroupForPid(pid_t pid) const
{
    return -1 != kill(-pid, SIGCONT);
}

} // namespace qtmir
