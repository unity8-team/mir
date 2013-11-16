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

#ifndef MIR_SURFACE_MANAGER_H
#define MIR_SURFACE_MANAGER_H

// std
#include <memory>

// Qt
#include <QObject>

// Mir
#include <mir_toolkit/common.h>

// local
#include "mirsurface.h"

class ShellServerConfiguration;
namespace mir { namespace shell { class Surface; class ApplicationSession; }}

class MirSurfaceManager : public QObject
{
    Q_OBJECT

public:
    static MirSurfaceManager* singleton();

    MirSurfaceManager(QObject *parent = 0);
    ~MirSurfaceManager();

    MirSurface *shellSurface() const;

    MirSurface *surfaceFor(std::shared_ptr<mir::shell::Surface> const& surface);

Q_SIGNALS:
    void surfaceCreated(MirSurface* surface);
    void surfaceDestroyed(MirSurface* surface);
    void shellSurfaceChanged(MirSurface* surface);
//    void surfaceResized(MirSurface*);
//    void fullscreenSurfaceChanged();

public Q_SLOTS:
    void sessionCreatedSurface(mir::shell::ApplicationSession const* session, std::shared_ptr<mir::shell::Surface> const&);
    void sessionDestroyingSurface(mir::shell::ApplicationSession const*, std::shared_ptr<mir::shell::Surface> const&);
    void shellSurfaceCreated(std::shared_ptr<mir::shell::Surface> const&);

    void surfaceAttributeChanged(mir::shell::Surface const*, MirSurfaceAttrib, int);

private:
    QHash<const mir::shell::Surface *, MirSurface *> m_surfaces;
    MirSurface* m_shellSurface;
    ShellServerConfiguration* m_mirServer;
    static MirSurfaceManager *the_surface_manager;
};

#endif // MIR_SURFACE_MANAGER_H
