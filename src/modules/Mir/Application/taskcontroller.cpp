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
 * Authored by: Ricardo Mendoza <ricardo.mendoza@canonical.com>
 */

// local
#include "taskcontroller.h"

// unity-mir
#include <logging.h>

// Qt
#include <QStringList>

// STL
#include <mutex>

// glib
#include <glib.h>

// std
#include <signal.h>
#include <unistd.h>

// linux specific
#include <linux/oom.h>

namespace
{
/**
 * From man proc:
 *
 * This file can be used to adjust the score used to select which
 * process should be killed in an out-of-memory (OOM) situation.  The
 * kernel uses this value for a bit-shift opera‐ tion of the process's
 * oom_score value: valid values are in the range -16 to +15, plus the
 * special value -17, which disables OOM-killing altogether for this
 * process.  A posi‐ tive score increases the likelihood of this process
 * being killed by the OOM-killer; a negative score decreases the
 * likelihood.
 *
 * The default value for this file is 0; a new process inherits its
 * parent's oom_adj setting.  A process must be privileged
 * (CAP_SYS_RESOURCE) to update this file.

 * Since Linux 2.6.36, use of this file is deprecated in favor of
 * /proc/[pid]/oom_score_adj.
 */
struct OomAdjuster
{
    static int disableOomKillerValue()
    {
        return OOM_DISABLE;
    }

    static int minValue()
    {
        return OOM_ADJUST_MIN;
    }

    static int maxValue()
    {
        return OOM_ADJUST_MAX;
    }

    static OomAdjuster leastLikelyToBeKilled()
    {
        // By system default, we set the oom_score_adj of Unity8 to -10 (via lightdm).
        // As we want to avoid that any app's oom_score_adj or oom_adj is <= Unity8's oom_score_adj,
        // we choose a default value of -9 for oom_score_adj and 0 for oom_adj.
        static const int default_value = 0;

        return OomAdjuster(default_value);
    }

    static OomAdjuster mostLikelyToBeKilled()
    {
        return OomAdjuster(maxValue());
    }

    OomAdjuster(int value) : value(value)
    {
    }

    bool isValid() const
    {
        return !(value < disableOomKillerValue() || value > maxValue());
    }

    bool applyForPid(pid_t pid) const
    {
        auto fn = QString("/proc/%1/oom_adj").arg(pid);
        QFile file(fn);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        QTextStream out(&file);
        out << value;

        return true;
    }

    int value;
};

/**
 * From man proc:
 *
 *  This file can be used to adjust the badness heuristic used to
 *  select which process gets killed in out-of-memory conditions.
 *
 *  The badness heuristic assigns a value to each candidate
 *  task ranging from 0 (never kill) to 1000 (always kill)
 *  to determine which process is targeted.  The units are
 *  roughly a proportion along that range of allowed memory
 *  the process may allocate from, based on an estimation of
 *  its current memory and swap use.  For example, if a task
 *  is using all allowed memory, its badness score will be
 *  1000.  If it is using half of its allowed memory, its
 *  score will be 500.
 *
 *  There is an additional factor included in the badness score: root
 *  processes are given 3% extra memory over other tasks.
 *
 *  The amount of "allowed" memory depends on the context in
 *  which the OOM-killer was called.  If it is due to the
 *  memory assigned to the allocating task's cpuset being
 *  exhausted, the allowed memory represents the set of mems
 *  assigned to that cpuset (see cpuset(7)).  If it is due
 *  to a mempolicy's node(s) being exhausted, the allowed
 *  memory represents the set of mempolicy nodes.  If it is
 *  due to a memory limit (or swap limit) being reached, the
 *  allowed memory is that configured limit.  Finally, if it
 *  is due to the entire system being out of memory, the
 *  allowed memory represents all allocatable resources.
 *
 *  The value of oom_score_adj is added to the badness score before it
 *  is used to determine which task to kill.  Acceptable values range
 *  from -1000 (OOM_SCORE_ADJ_MIN) to +1000 (OOM_SCORE_ADJ_MAX).  This
 *  allows user space to control the preference for OOM-killing,
 *  ranging from always preferring a certain task or completely
 *  disabling it from OOM- killing.  The lowest possible value, -1000,
 *  is equivalent to disabling OOM-killing entirely for that task,
 *  since it will always report a badness score of 0.
 *
 *  Consequently, it is very simple for user space to define the amount
 *  of memory to consider for each task.  Setting a oom_score_adj value
 *  of +500, for example, is roughly equiv‐ alent to allowing the
 *  remainder of tasks sharing the same system, cpuset, mempolicy, or
 *  memory controller resources to use at least 50% more memory.  A
 *  value of -500, on the other hand, would be roughly equivalent to
 *  discounting 50% of the task's allowed memory from being considered
 *  as scoring against the task.
 *
 *  For backward compatibility with previous kernels,
 *  /proc/[pid]/oom_adj can still be used to tune the badness score.
 *  Its value is scaled linearly with oom_score_adj.
 *
 *  Writing to /proc/[pid]/oom_score_adj or
 *  /proc/[pid]/oom_adj will change the other with its
 *  scaled value.
 */
struct OomScoreAdjuster
{
    static int disableOomKillerValue()
    {
        return OOM_SCORE_ADJ_MIN;
    }

    static int minValue()
    {
        return OOM_SCORE_ADJ_MIN;
    }

    static int maxValue()
    {
        return OOM_SCORE_ADJ_MAX;
    }

    static OomScoreAdjuster leastLikelyToBeKilled()
    {
        // By system default, we set the oom_score_adj of Unity8 to -10 (via lightdm).
        // As we want to avoid that any app's oom_score_adj is <= Unity8's oom_score_adj,
        // we choose a default value of -9, and a default increase of +1.
        static const int default_value = -9;
        static const int default_increase = 1;
        

        // We could be way more clever here if we knew the distribution
        // of oom_score_adj values of all app processes. However, we just
        // make sure that the process is not ignored by the oom killer for now.
        return OomScoreAdjuster(
            OomScoreAdjuster::thisProcess().isValid() ?
            OomScoreAdjuster::thisProcess().value + default_increase :
            default_value);
    }

    static OomScoreAdjuster mostLikelyToBeKilled()
    {
        // We avoid extremal values for oom_score_adj. For that, we
        // set it to 80% of the total available range. If we cannot
        // determine the oom_score_adj of the current process, i.e.,
        // Unity8, we substract -200 by default.
        static const float default_percentage = 0.8;
        static const int default_decrease = -200;

        return OomScoreAdjuster(
            OomScoreAdjuster::thisProcess().isValid() ?
            (maxValue() - OomScoreAdjuster::thisProcess().value) * default_percentage + OomScoreAdjuster::thisProcess().value :
            maxValue() - default_decrease);
    }

    static const OomScoreAdjuster& thisProcess()
    {
        // Initialize as invalid.
        static OomScoreAdjuster adjusterForThisProcess(minValue()-1);

        static std::once_flag query_once;
        std::call_once(
            query_once,
            []()
            {
                pid_t pid = getpid();

                auto fn = QString("/proc/%1/oom_score_adj").arg(pid);
                QFile file(fn);

                if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
                    return;

                QTextStream in(&file);
                int value; in >> value;

                adjusterForThisProcess.value = value;
            });

        return adjusterForThisProcess;
    }

    OomScoreAdjuster(int value) : value(value)
    {
    }

    bool isValid() const
    {
        return !(value < disableOomKillerValue() || value > maxValue());
    }

    bool applyForPid(pid_t pid) const
    {
        auto fn = QString("/proc/%1/oom_score_adj").arg(pid);
        QFile file(fn);

        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;

        QTextStream out(&file);
        out << value;

        return true;
    }

    int value;
};

void ensureProcessIsUnlikelyToBeKilled(pid_t pid)
{
    if (!OomScoreAdjuster::leastLikelyToBeKilled().applyForPid(pid))
        if (!OomAdjuster::leastLikelyToBeKilled().applyForPid(pid))
            LOG("ensureProcessIsUnlikelyToBeKilled failed");
}

void ensureProcessIsLikelyToBeKilled(pid_t pid)
{
    if (!OomScoreAdjuster::mostLikelyToBeKilled().applyForPid(pid))
        if (!OomAdjuster::mostLikelyToBeKilled().applyForPid(pid))
            LOG("ensureProcessIsLikelyToBeKilled failed");
}
}

TaskController* TaskController::m_theTaskController = nullptr;

TaskController* TaskController::singleton()
{
    if (!m_theTaskController) {
        m_theTaskController = new TaskController();
    }
    return m_theTaskController;
}

TaskController::TaskController(QObject *parent) :
    QObject(parent)
{
    startCallback = [](const gchar * appId, gpointer userData) {
        Q_UNUSED(userData)
        pid_t pid = upstart_app_launch_get_primary_pid(appId);
        ensureProcessIsUnlikelyToBeKilled(pid);
        Q_EMIT TaskController::singleton()->processStartReport(QString(appId), false);
    };

    stopCallback = [](const gchar * appId, gpointer userData) {
        Q_UNUSED(userData)
        Q_EMIT TaskController::singleton()->processStopped(QString(appId), false);
    };

    focusCallback = [](const gchar * appId, gpointer userData) {
        Q_UNUSED(userData)
        pid_t pid = upstart_app_launch_get_primary_pid(appId);
        ensureProcessIsUnlikelyToBeKilled(pid);
        Q_EMIT TaskController::singleton()->requestFocus(QString(appId));
    };

    resumeCallback = [](const gchar * appId, gpointer userData) {
        Q_UNUSED(userData)
        Q_EMIT TaskController::singleton()->requestResume(QString(appId));
    };

    failureCallback = [](const gchar * appId, upstart_app_launch_app_failed_t failureType, gpointer userData) {
        Q_UNUSED(userData)
        if (failureType == UPSTART_APP_LAUNCH_APP_FAILED_CRASH) {
            Q_EMIT TaskController::singleton()->processStopped(QString(appId), true);
        } else if (failureType == UPSTART_APP_LAUNCH_APP_FAILED_START_FAILURE) {
            Q_EMIT TaskController::singleton()->processStartReport(QString(appId), true);
        } else {
            LOG("TaskController: unknown failure type returned from upstart-app-launch");
        }
        Q_EMIT TaskController::singleton()->requestResume(QString(appId));
    };

    upstart_app_launch_observer_add_app_start(startCallback, nullptr);
    upstart_app_launch_observer_add_app_stop(stopCallback, nullptr);
    upstart_app_launch_observer_add_app_focus(focusCallback, nullptr);
    upstart_app_launch_observer_add_app_resume(resumeCallback, nullptr);
    upstart_app_launch_observer_add_app_failed(failureCallback, nullptr);
}

TaskController::~TaskController()
{
    upstart_app_launch_observer_delete_app_start(startCallback, nullptr);
    upstart_app_launch_observer_delete_app_stop(stopCallback, nullptr);
    upstart_app_launch_observer_delete_app_focus(focusCallback, nullptr);
    upstart_app_launch_observer_delete_app_resume(resumeCallback, nullptr);
    upstart_app_launch_observer_delete_app_failed(failureCallback, nullptr);
}

bool TaskController::start(const QString& appId, const QStringList& arguments)
{
    DLOG("TaskController::start appId='%s'", qPrintable(appId));
    gchar ** upstartArgs = nullptr;
    bool result = false;

    // Convert arguments QStringList into format suitable for upstart-app-launch
    upstartArgs = g_new0(gchar *, arguments.length());

    for (int i=0; i<arguments.length(); i++) {
        upstartArgs[i] = arguments.at(i).toLatin1().data();
    }

    result = upstart_app_launch_start_application(appId.toLatin1().constData(),
                                                  static_cast<const gchar * const *>(upstartArgs));
    g_free(upstartArgs);

    DLOG_IF(!result, "TaskController::startApplication appId='%s' FAILED", qPrintable(appId));
    return result;
}

bool TaskController::stop(const QString& appId)
{
    DLOG("TaskController::stop appId='%s'", qPrintable(appId));
    bool result = false;

    result = upstart_app_launch_stop_application(appId.toLatin1().constData());

    DLOG_IF(!result, "TaskController::stopApplication appId='%s' FAILED", qPrintable(appId));
    return result;
}

bool TaskController::appIdHasProcessId(const QString& appId, const quint64 pid)
{
    DLOG("TaskController::isApplicationPid appId='%s', pid=%lld", qPrintable(appId), pid);
    return upstart_app_launch_pid_in_app_id(pid, appId.toLatin1().constData());
}

bool TaskController::suspend(const QString& appId)
{
    DLOG("TaskController::suspend (this=%p, application=%p)", this, qPrintable(appId));
    pid_t pid = upstart_app_launch_get_primary_pid(appId.toLatin1().constData());

    ensureProcessIsLikelyToBeKilled(pid);

    if (pid) {
        // We do assume that the app was launched by upstart and with that,
        // in its own process group. For that, we interpret the pid as pgid and
        // sigstop the complete process group on suspend.
        kill(-pid, SIGSTOP);
        return true;
    } else {
        return false;
    }
}

bool TaskController::resume(const QString& appId)
{
    DLOG("TaskController::resume (this=%p, application=%p)", this, qPrintable(appId));
    pid_t pid = upstart_app_launch_get_primary_pid(appId.toLatin1().constData());

    ensureProcessIsUnlikelyToBeKilled(pid);

    if (pid) {
        // We do assume that the app was launched by upstart and with that,
        // in its own process group. For that, we interpret the pid as pgid and
        // sigcont the complete process group on resume.
        kill(-pid, SIGCONT);
        return true;
    } else {
        return false;
    }
}
