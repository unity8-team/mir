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
 */

#ifndef MIR_SESSION_MANAGER_H
#define MIR_SESSION_MANAGER_H

// std
#include <memory>

// Qt
#include <QHash>
#include <QMutex>

// Mir
#include <mir_toolkit/common.h>

// local
#include "mirsessionitem.h"
#include "mirsessionitemmodel.h"

namespace mir {
    namespace scene {
        class Session;
        class PromptSession;
    }
}

class MirServerConfiguration;

namespace qtmir {

class Application;
class ApplicationManager;

class MirSessionManager : public MirSessionItemModel
{
    Q_OBJECT

public:
    static MirSessionManager* singleton();

    ~MirSessionManager();

    MirSessionItem *findSession(const mir::scene::Session* session) const;

Q_SIGNALS:
    void sessionStarting(MirSessionItem* session);
    void sessionStopping(MirSessionItem* session);

public Q_SLOTS:
    void onSessionStarting(std::shared_ptr<mir::scene::Session> const& session);
    void onSessionStopping(std::shared_ptr<mir::scene::Session> const& session);

    void onPromptProviderAdded(const mir::scene::PromptSession *, const std::shared_ptr<mir::scene::Session> &);
    void onPromptProviderRemoved(const mir::scene::PromptSession *, const std::shared_ptr<mir::scene::Session> &);

protected:
    MirSessionManager(
        const QSharedPointer<MirServerConfiguration>& mirConfig,
        ApplicationManager* applicationManager,
        QObject *parent = 0
    );
private:
    QSharedPointer<MirServerConfiguration> m_mirConfig;
    ApplicationManager* m_applicationManager;
    static MirSessionManager *the_session_manager;

    QList<MirSessionItem*> m_sessions;
};

} // namespace qtmir

#endif // MIR_SESSION_MANAGER_H
