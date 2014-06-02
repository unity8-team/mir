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
#include <QAbstractListModel>
#include <QHash>

// Mir
#include <mir_toolkit/common.h>

// local
#include "mirsurfaceitem.h"

class ShellServerConfiguration;
namespace mir { namespace shell { class Surface; }}
namespace mir { namespace scene { class Surface; class Session; }}

class MirSurfaceManager : public QAbstractListModel
{
    Q_OBJECT

    Q_ENUMS(Roles)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        RoleSurface = Qt::UserRole,
    };

    static MirSurfaceManager* singleton();

    MirSurfaceManager(QObject *parent = 0);
    ~MirSurfaceManager();

    // from QAbstractItemModel
    int rowCount(const QModelIndex & parent = QModelIndex()) const override;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override { return m_roleNames; }

    int count() const { return rowCount(); }

    Q_INVOKABLE MirSurfaceItem* getSurface(int index);

Q_SIGNALS:
    void countChanged();
    void surfaceCreated(MirSurfaceItem* surface);
    void surfaceDestroyed(MirSurfaceItem* surface);
//    void surfaceResized(MirSurface*);
//    void fullscreenSurfaceChanged();

public Q_SLOTS:
    void onSessionCreatedSurface(mir::scene::Session const* session, std::shared_ptr<mir::scene::Surface> const&);
    void onSessionDestroyingSurface(mir::scene::Session const*, std::shared_ptr<mir::scene::Surface> const&);

    void onSurfaceAttributeChanged(mir::scene::Surface const*, MirSurfaceAttrib, int);

private:
    QHash<const mir::scene::Surface *, MirSurfaceItem *> m_mirSurfaceToItemHash;
    QList<MirSurfaceItem*> m_surfaceItems;
    ShellServerConfiguration* m_mirServer;
    static MirSurfaceManager *the_surface_manager;
    QHash<int, QByteArray> m_roleNames;
};

#endif // MIR_SURFACE_MANAGER_H
