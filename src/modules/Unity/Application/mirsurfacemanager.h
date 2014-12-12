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

class MirServer;

namespace qtmir {

class Application;
class ApplicationManager;
class SessionManager;

class MirSurfaceManager : public MirSurfaceItemModel
{
    Q_OBJECT

public:
    explicit MirSurfaceManager(
        const QSharedPointer<MirServer>& mirServer,
        SessionManager* sessionManager,
        QObject *parent = 0
    );
    ~MirSurfaceManager();

    static MirSurfaceManager* singleton();

Q_SIGNALS:
    void surfaceCreated(MirSurfaceItem* surface);
    void surfaceDestroyed(MirSurfaceItem* surface);
//    void surfaceResized(MirSurface*);
//    void fullscreenSurfaceChanged();

public Q_SLOTS:
    void onSessionCreatedSurface(const mir::scene::Session *, const std::shared_ptr<mir::scene::Surface> &, std::shared_ptr<mir::scene::SurfaceObserver> const&);
    void onSessionDestroyingSurface(const mir::scene::Session *, const std::shared_ptr<mir::scene::Surface> &);

    void onSurfaceAttributeChanged(const mir::scene::Surface *, MirSurfaceAttrib, int);

protected:

    QHash<const mir::scene::Surface *, MirSurfaceItem *> m_mirSurfaceToItemHash;
    QMutex m_mutex;

private:
    QSharedPointer<MirServer> m_mirServer;
    SessionManager* m_sessionManager;
    static MirSurfaceManager *the_surface_manager;
};

} // namespace qtmir

#endif // MIR_SURFACE_MANAGER_H
