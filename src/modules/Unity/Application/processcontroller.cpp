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

/*
 * ProcessController
 * This class manages individual processes during their lifetime. It allows one to pause
 * and resume processes.
 *
 * It also has methods to set a score on processes which is used by the Out of Memory (OOM)
 * process killer to decide what process gets killed in a low memory situation.
 */

namespace plpp = core::posix::linux::proc::process;

namespace qtmir
{

ProcessController::OomController::OomController()
{
}

/*!
 * \brief ProcessController::OomController::ensureProcessLikelyToBeKilled
 * \param pid
 * Set the process OOM-killer score so that the process is _more_ likely to be killed
 * in a low memory situation
 */

/*
 * Useful information about these OOM-weighting methods
 *
 * the "Process C++" library wraps some APIs exported by the kernel in the the /proc filesystem.
 * The OOM score is one of these, but the APIs for it changed from Linux 2.6.36 onward.
 *
 * === Up to Linux 2.6.36 ===
 * /proc/[pid]/oom_adj (wrapped by plpp::OomAdj) was used up to 2.6.36 (is deprecated now)
 * Valid values are in the range -16 to +15, plus the special value -17, which disables
 * OOM-killing altogether for this process.  A positive score increases the likelihood
 * of this process being killed by the OOM-killer; a negative score decreases the likelihood.
 *
 * The default value for this file is 0; a new process inherits its parent's oom_adj setting.
 *
 * === Linux 2.6.36 and later ===
 * /proc/[pid]/oom_score_adj (wrapped by plpp::OomScoreAdj) is the recommended API.  Valid
 * values are in the range -1000 (never kill) to 1000 (always kill).
 *
 * For backward compatibility with previous kernels, /proc/[pid]/oom_adj can still be used to
 * tune the badness score.  Its value is scaled linearly with oom_score_adj. So writing to
 * /proc/[pid]/oom_score_adj or /proc/[pid]/oom_adj will change the other with its scaled value.
 *
 * === How OOM score changing is implemented ===
 * To support all available kernels, we thus perform the following for any OOM score change:
 *  1. Attempt to use the newer kernel API (plpp::OomScoreAdj). Fetch the min & max possible
 *     values and calculate a desired score for the process.
 *  2. If that fails, fall back to the older kernel API (plpp::OomSAdj), again fetching min
 *     & max possible values and calculating a desired score for the process.
 */
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

/*!
 * \brief ProcessController::OomController::ensureProcessLessLikelyToBeKilled
 * \param pid
 * Set the process OOM-killer score so that, in a low memory situation, the process
 * is _more_ likely to be killed than the foreground application, but less likely
 * than the background applications
 */
void ProcessController::OomController::ensureProcessLessLikelyToBeKilled(pid_t pid)
{
    // Set it to 50% of the total available range.
    static const float defaultPercentage = 0.5;

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
            qDebug() << "ensureProcessIsLessLikelyToBeKilled failed for pid=" << pid;
        }
    }
}

/*!
 * \brief ProcessController::OomController::ensureProcessUnlikelyToBeKilled
 * \param pid
 * Set the process OOM-killer weighting so that the process is _less_ likely to be killed
 * in a low memory situation
 */
void ProcessController::OomController::ensureProcessUnlikelyToBeKilled(pid_t pid)
{
    // By system default, we set the oom_score_adj of Unity8 to -10 (via lightdm).
    // As we want to avoid that any app's oom_score_adj is <= Unity8's oom_score_adj,
    // (i.e. Unity8 would be killed before that app is) we choose a default increase of +1.
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

/*!
 * \brief ProcessController::sigStopProcessGroupForPid
 * \param pid
 * \return True if process was stopped successfully, false otherwise
 */
bool ProcessController::sigStopProcessGroupForPid(pid_t pid) const
{
    return -1 != kill(-pid, SIGSTOP);
}

/*!
 * \brief ProcessController::sigContinueProcessGroupForPid
 * \param pid
 * \return True of process was resumed successfully, false otherwise
 */
bool ProcessController::sigContinueProcessGroupForPid(pid_t pid) const
{
    return -1 != kill(-pid, SIGCONT);
}

} // namespace qtmir
