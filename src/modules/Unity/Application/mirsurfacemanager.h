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

#ifndef MIR_SURFACE_MANAGER_H
#define MIR_SURFACE_MANAGER_H

// std
#include <memory>

// Qt
#include <QHash>
#include <QMutex>

// Mir
#include <mir_toolkit/common.h>

// local
#include "mirsurfaceitem.h"
#include "mirsurfaceitemmodel.h"

namespace mir {
    namespace scene {
        class Surface;
        class Session;
        class PromptSession;
    }
}

class MirServerConfiguration;

namespace qtmir {

class Application;
class ApplicationManager;

class MirSurfaceManager : public MirSurfaceItemModel
{
    Q_OBJECT

public:
    static MirSurfaceManager* singleton();

    ~MirSurfaceManager();

Q_SIGNALS:
    void surfaceCreated(MirSurfaceItem* surface);
    void surfaceDestroyed(MirSurfaceItem* surface);
//    void surfaceResized(MirSurface*);
//    void fullscreenSurfaceChanged();

public Q_SLOTS:
    void onSessionCreatedSurface(const mir::scene::Session *, const std::shared_ptr<mir::scene::Surface> &);
    void onSessionDestroyingSurface(const mir::scene::Session *, const std::shared_ptr<mir::scene::Surface> &);

    void onSurfaceAttributeChanged(const mir::scene::Surface *, MirSurfaceAttrib, int);

    void onPromptProviderAdded(const mir::scene::PromptSession *, const std::shared_ptr<mir::scene::Session> &);
    void onPromptProviderRemoved(const mir::scene::PromptSession *, const std::shared_ptr<mir::scene::Session> &);

protected:
    MirSurfaceManager(
        const QSharedPointer<MirServerConfiguration>& mirConfig,
        ApplicationManager* applicationManager,
        QObject *parent = 0
    );

    void refreshPromptSessionSurfaces(const mir::scene::Session *session);
    void refreshPromptSessionSurfaces(Application* application);

    QHash<const mir::scene::Surface *, MirSurfaceItem *> m_mirSurfaceToItemHash;
    QMultiHash<const mir::scene::Session *, MirSurfaceItem *> m_mirSessionToItemHash;
    QMutex m_mutex;

private:
    QSharedPointer<MirServerConfiguration> m_mirConfig;
    ApplicationManager* m_applicationManager;
    static MirSurfaceManager *the_surface_manager;
};

} // namespace qtmir

#endif // MIR_SURFACE_MANAGER_H
