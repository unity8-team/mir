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
class MirSessionItem;

class Application : public unity::shell::application::ApplicationInfoInterface
{
    Q_OBJECT

    Q_FLAGS(Orientation SupportedOrientations)

    Q_PROPERTY(QString desktopFile READ desktopFile CONSTANT)
    Q_PROPERTY(QString exec READ exec CONSTANT)
    Q_PROPERTY(bool fullscreen READ fullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(Stage stage READ stage WRITE setStage NOTIFY stageChanged)
    Q_PROPERTY(SupportedOrientations supportedOrientations READ supportedOrientations CONSTANT)
    Q_PROPERTY(MirSessionItem* session READ session NOTIFY sessionChanged DESIGNABLE false)

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

    MirSessionItem* session() const;

    bool canBeResumed() const;
    void setCanBeResumed(const bool);

    bool isValid() const;
    QString desktopFile() const;
    QString exec() const;
    bool fullscreen() const;
    std::shared_ptr<mir::scene::PromptSession> activePromptSession() const;
    void foreachPromptSession(std::function<void(const std::shared_ptr<mir::scene::PromptSession>&)> f) const;

    Stages supportedStages() const;
    SupportedOrientations supportedOrientations() const;

    bool containsProcess(pid_t pid) const;

public Q_SLOTS:
    void suspend();
    void resume();
    void respawn();

Q_SIGNALS:
    void fullscreenChanged(bool fullscreen);
    void stageChanged(Stage stage);
    void sessionChanged(MirSessionItem *session);

private:
    QString longAppId() const;
    pid_t pid() const;
    void setPid(pid_t pid);
    void setState(State state);
    void setFocused(bool focus);
    void setSession(MirSessionItem *session);

    void appendPromptSession(const std::shared_ptr<mir::scene::PromptSession>& session);
    void removePromptSession(const std::shared_ptr<mir::scene::PromptSession>& session);
    void stopPromptSessions();

    QSharedPointer<TaskController> m_taskController;
    DesktopFileReader* m_desktopData;
    QString m_longAppId;
    qint64 m_pid;
    Stage m_stage;
    Stages m_supportedStages;
    State m_state;
    bool m_focused;
    bool m_canBeResumed;
    QStringList m_arguments;
    SupportedOrientations m_supportedOrientations;
    MirSessionItem *m_session;
    QList<std::shared_ptr<mir::scene::PromptSession>> m_promptSessions;
    std::shared_ptr<mir::scene::PromptSessionManager> const m_promptSessionManager;

    friend class ApplicationManager;
    friend class MirSessionManager;
    friend class MirSessionItem;
};

} // namespace qtmir

Q_DECLARE_METATYPE(qtmir::Application*)
Q_DECLARE_OPERATORS_FOR_FLAGS(qtmir::Application::SupportedOrientations)

#endif  // APPLICATION_H
