/*
 * Copyright (C) 2013-2014 Canonical, Ltd.
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

#ifndef APPLICATION_H
#define APPLICATION_H

// std
#include <memory>

//Qt
#include <QtCore/QtCore>
#include <QImage>
#include <QSharedPointer>

// Unity API
#include <unity/shell/application/ApplicationInfoInterface.h>

// local
#include "mirsurfaceitem.h"

namespace mir {
    namespace scene {
        class Session;
        class PromptSession;
        class PromptSessionManager;
    }
}

namespace qtmir
{

class ApplicationManager;
class DesktopFileReader;
class TaskController;

class Application : public unity::shell::application::ApplicationInfoInterface
{
    Q_OBJECT

    Q_FLAGS(Orientation SupportedOrientations)

    Q_PROPERTY(QString desktopFile READ desktopFile CONSTANT)
    Q_PROPERTY(QString exec READ exec CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(Stage stage READ stage WRITE setStage NOTIFY stageChanged)
    Q_PROPERTY(SupportedOrientations supportedOrientations READ supportedOrientations CONSTANT)
    Q_PROPERTY(MirSurfaceItem* surface READ surface NOTIFY surfaceChanged)
    Q_PROPERTY(QQmlListProperty<qtmir::MirSurfaceItem> promptSurfaces READ promptSurfaces NOTIFY promptSurfacesChanged DESIGNABLE false)

public:
    Q_DECLARE_FLAGS(Stages, Stage)

    // Matching Qt::ScreenOrientation values for convenience
    enum Orientation {
        PortraitOrientation = 0x1,
        LandscapeOrientation = 0x2,
        InvertedPortraitOrientation = 0x4,
        InvertedLandscapeOrientation = 0x8
    };
    Q_DECLARE_FLAGS(SupportedOrientations, Orientation)

    Application(const QSharedPointer<TaskController>& taskController,
                DesktopFileReader *desktopFileReader,
                State state,
                const QStringList &arguments,
                const std::shared_ptr<mir::scene::PromptSessionManager>& promptSessionManager,
                ApplicationManager *parent);
    virtual ~Application();

    // ApplicationInfoInterface
    QString appId() const override;
    QString name() const override;
    QString comment() const override;
    QUrl icon() const override;
    Stage stage() const override;
    State state() const override;
    bool focused() const override;

    void setStage(Stage stage);

    MirSurfaceItem* surface() const;

    bool canBeResumed() const;
    void setCanBeResumed(const bool);

    bool isValid() const;
    QString desktopFile() const;
    QString exec() const;
    bool fullscreen() const;
    std::shared_ptr<mir::scene::Session> session() const;
    std::shared_ptr<mir::scene::PromptSession> activePromptSession() const;
    void foreachPromptSession(std::function<void(const std::shared_ptr<mir::scene::PromptSession>&)> f) const;

    Stages supportedStages() const;
    SupportedOrientations supportedOrientations() const;

    bool containsProcess(pid_t pid) const;

    void foreachPromptSurface(std::function<void(MirSurfaceItem*)> f) const;
    void addPromptSurface(MirSurfaceItem* surface);
    void removeSurface(MirSurfaceItem* surface);

public Q_SLOTS:
    void suspend();
    void resume();
    void respawn();

Q_SIGNALS:
    void fullscreenChanged();
    void stageChanged(Stage stage);
    void surfaceChanged();
    void promptSurfacesChanged();

    void surfaceDestroyed(MirSurfaceItem *surface);

private Q_SLOTS:
    void discardSurface();
    void emitSurfaceChanged();

private:
    QString longAppId() const;
    pid_t pid() const;
    void setPid(pid_t pid);
    void setState(State state);
    void setFocused(bool focus);
    void setFullscreen(bool fullscreen);
    void setSession(const std::shared_ptr<mir::scene::Session>& session);
    void setSurface(MirSurfaceItem *surface);

    void appendPromptSession(const std::shared_ptr<mir::scene::PromptSession>& session);
    void removePromptSession(const std::shared_ptr<mir::scene::PromptSession>& session);
    void stopPromptSessions();

    void updateFullscreenProperty();

    QQmlListProperty<MirSurfaceItem> promptSurfaces();
    static int promptSurfaceCount(QQmlListProperty<MirSurfaceItem> *prop);
    static MirSurfaceItem* promptSurfaceAt(QQmlListProperty<MirSurfaceItem> *prop, int index);

    ApplicationManager* m_appMgr;
    QSharedPointer<TaskController> m_taskController;
    DesktopFileReader* m_desktopData;
    QString m_longAppId;
    qint64 m_pid;
    Stage m_stage;
    Stages m_supportedStages;
    State m_state;
    bool m_focused;
    bool m_canBeResumed;
    bool m_fullscreen;
    std::shared_ptr<mir::scene::Session> m_session;
    QStringList m_arguments;
    QTimer* m_suspendTimer;
    SupportedOrientations m_supportedOrientations;
    MirSurfaceItem *m_surface;
    QList<MirSurfaceItem*> m_promptSurfaces;
    QList<std::shared_ptr<mir::scene::PromptSession>> m_promptSessions;
    std::shared_ptr<mir::scene::PromptSessionManager> const m_promptSessionManager;

    class Guard {};
    QSharedPointer<Guard> m_screenShotGuard;

    friend class ApplicationManager;
    friend class MirSurfaceManager;
    friend class MirSurfaceItem;
};

} // namespace qtmir

Q_DECLARE_METATYPE(qtmir::Application*)
Q_DECLARE_OPERATORS_FOR_FLAGS(qtmir::Application::SupportedOrientations)

#endif  // APPLICATION_H
