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
#include "desktopfilereader.h"

namespace mir {
    namespace scene {
        class Session;
        class Surface;
    }
}

class MirServerConfiguration;

namespace qtmir {

class DBusWindowStack;
class MirSurfaceManager;
class ProcInfo;
class TaskController;

class ApplicationManager : public unity::shell::application::ApplicationManagerInterface
{
    Q_OBJECT
    Q_ENUMS(MoreRoles)
    Q_FLAGS(ExecFlags)
    Q_PROPERTY(qtmir::Application* topmostApplication READ topmostApplication NOTIFY topmostApplicationChanged)
    Q_PROPERTY(bool empty READ isEmpty NOTIFY emptyChanged)

public:
    class Factory
    {
    public:
        ApplicationManager* create();
    };

    enum MoreRoles {
        RoleSurface = RoleScreenshot+1,
        RoleFullscreen,
    };

    // Mapping enums to Ubuntu Platform API enums.
    enum Flag {
        NoFlag = 0x0,
        ForceMainStage = 0x1,
    };
    Q_DECLARE_FLAGS(ExecFlags, Flag)

    static ApplicationManager* singleton();

    explicit ApplicationManager(
            const QSharedPointer<MirServerConfiguration> &mirConfig,
            const QSharedPointer<TaskController> &taskController,
            const QSharedPointer<DesktopFileReader::Factory> &desktopFileReaderFactory,
            const QSharedPointer<ProcInfo> &processInfo,
            QObject *parent = 0);
    virtual ~ApplicationManager();

    // ApplicationManagerInterface
    QString focusedApplicationId() const override;
    bool suspended() const override;
    void setSuspended(bool suspended) override;
    Q_INVOKABLE qtmir::Application* get(int index) const override;
    Q_INVOKABLE qtmir::Application* findApplication(const QString &appId) const override;
    Q_INVOKABLE bool requestFocusApplication(const QString &appId) override;
    Q_INVOKABLE bool focusApplication(const QString &appId) override;
    Q_INVOKABLE void unfocusCurrentApplication() override;
    Q_INVOKABLE qtmir::Application* startApplication(const QString &appId, const QStringList &arguments) override;
    Q_INVOKABLE bool stopApplication(const QString &appId) override;
    Q_INVOKABLE bool updateScreenshot(const QString &appId) override;

    // QAbstractListModel
    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;

    qtmir::Application* topmostApplication() const;

    Q_INVOKABLE qtmir::Application *startApplication(const QString &appId, ExecFlags flags,
                                              const QStringList &arguments = QStringList());
    Q_INVOKABLE void move(int from, int to);

    bool isEmpty() const { return rowCount() == 0; }

    const QList<Application*> &list() const { return m_applications; }
    qtmir::Application* findApplicationWithPid(const qint64 pid);

public Q_SLOTS:
    void authorizeSession(const quint64 pid, bool &authorized);

    void onSessionStarting(std::shared_ptr<mir::scene::Session> const& session);
    void onSessionStopping(std::shared_ptr<mir::scene::Session> const& session);

    void onSessionCreatedSurface(mir::scene::Session const*, std::shared_ptr<mir::scene::Surface> const&);

    void onProcessStarting(const QString& appId);
    void onProcessStopped(const QString& appId);
    void onProcessFailed(const QString& appId, const bool duringStartup);
    void onFocusRequested(const QString& appId);
    void onResumeRequested(const QString& appId);

Q_SIGNALS:
    void focusRequested(const QString &appId);
    void topmostApplicationChanged(Application *application);
    void emptyChanged();

private Q_SLOTS:
    void screenshotUpdated();

private:
    void setFocused(Application *application);
    void add(Application *application);
    void remove(Application* application);
    Application* findApplicationWithSession(const std::shared_ptr<mir::scene::Session> &session);
    Application* findApplicationWithSession(const mir::scene::Session *session);
    Application* applicationForStage(Application::Stage stage);
    QModelIndex findIndex(Application* application);
    bool suspendApplication(Application *application);
    void resumeApplication(Application *application);
    QString toString() const;

    QSharedPointer<MirServerConfiguration> m_mirConfig;

    QList<Application*> m_applications;
    Application* m_focusedApplication; // remove as Mir has API for this
    Application* m_mainStageApplication;
    Application* m_sideStageApplication;
    QStringList m_lifecycleExceptions;
    DBusWindowStack* m_dbusWindowStack;
    QSharedPointer<TaskController> m_taskController;
    QSharedPointer<DesktopFileReader::Factory> m_desktopFileReaderFactory;
    QSharedPointer<ProcInfo> m_procInfo;
    static ApplicationManager* the_application_manager;
    bool m_fenceNext;
    QString m_nextFocusedAppId;
    bool m_suspended;

    friend class Application;
    friend class DBusWindowStack;
    friend class MirSurfaceManager;
};

} // namespace qtmir

Q_DECLARE_METATYPE(qtmir::ApplicationManager*)

#endif // APPLICATIONMANAGER_H
