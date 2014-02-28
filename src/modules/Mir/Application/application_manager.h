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
 */

#ifndef APPLICATIONMANAGER_H
#define APPLICATIONMANAGER_H

// std
#include <memory>

// Qt
#include <QObject>
#include <QStringList>

// Unity API
#include <unity/shell/application/ApplicationManagerInterface.h>

// local
#include "application.h"

class MirServerConfiguration;
class DBusWindowStack;
class MirSurfaceManager;
class TaskController;

namespace mir {
    namespace shell {
        class Session;
        class Surface;
    }
}

class ApplicationManager : public unity::shell::application::ApplicationManagerInterface
{
    Q_OBJECT
    Q_FLAGS(ExecFlags)

public:
    // Mapping enums to Ubuntu Platform API enums.
    enum Flag {
        NoFlag = 0x0,
        ForceMainStage = 0x1,
    };
    Q_DECLARE_FLAGS(ExecFlags, Flag)

    static ApplicationManager* singleton();

    explicit ApplicationManager(QObject *parent = 0);
    virtual ~ApplicationManager();

    // ApplicationManagerInterface
    QString focusedApplicationId() const override;
    bool suspended() const override;
    void setSuspended(bool suspended) override;
    Q_INVOKABLE Application* get(int index) const override;
    Q_INVOKABLE Application* findApplication(const QString &appId) const override;
    Q_INVOKABLE void activateApplication(const QString &appId) override;
    Q_INVOKABLE bool focusApplication(const QString &appId) override;
    Q_INVOKABLE void unfocusCurrentApplication() override;
    Q_INVOKABLE Application* startApplication(const QString &appId, const QStringList &arguments) override;
    Q_INVOKABLE bool stopApplication(const QString &appId) override;
    Q_INVOKABLE void updateScreenshot(const QString &appId) override;

    // QAbstractListModel
    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

    Q_INVOKABLE Application *startApplication(const QString &appId, ExecFlags flags,
                                              const QStringList &arguments = QStringList());
    Q_INVOKABLE void move(int from, int to);

    const QList<Application*> &list() const { return m_applications; }
    Application* findApplicationWithPid(const qint64 pid);

public Q_SLOTS:
    void authorizeSession(const quint64 pid, bool &authorized);

    void onSessionStarting(std::shared_ptr<mir::shell::Session> const& session);
    void onSessionStopping(std::shared_ptr<mir::shell::Session> const& session);
    void onSessionFocused(std::shared_ptr<mir::shell::Session> const& session);
    void onSessionUnfocused();

    void onSessionCreatedSurface(mir::shell::Session const*, std::shared_ptr<mir::shell::Surface> const&);

    void onProcessStartReportReceived(const QString& appId, const bool failure);
    void onProcessStopped(const QString& appId, const bool unexpected);
    void onFocusRequested(const QString& appId);
    void onResumeRequested(const QString& appId);

Q_SIGNALS:
    void focusRequested(const QString &appId);

private Q_SLOTS:
    void screenshotUpdated();

private:
    void setFocused(Application *application);
    void add(Application *application);
    void remove(Application* application);
    Application* findApplicationWithSession(const std::shared_ptr<mir::shell::Session> &session);
    Application* findApplicationWithSession(const mir::shell::Session *session);
    Application* findLastExecutedApplication();
    QModelIndex findIndex(Application* application);
    void suspendApplication(Application *application);
    void resumeApplication(Application *application);

    QList<Application*> m_applications;
    Application* m_focusedApplication; // remove as Mir has API for this
    Application* m_applicationToBeFocused; // a basic form of focus stealing prevention
    QStringList m_lifecycleExceptions;
    MirServerConfiguration* m_mirServer;
    DBusWindowStack* m_dbusWindowStack;
    QScopedPointer<TaskController> m_taskController;
    static ApplicationManager* the_application_manager;
    bool m_fenceNext;
    QString m_nextFocusedAppId;
    bool m_suspended;

    friend class DBusWindowStack;
    friend class MirSurfaceManager;
};

Q_DECLARE_METATYPE(ApplicationManager*)

#endif // APPLICATIONMANAGER_H
