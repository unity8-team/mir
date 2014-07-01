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
#include <QAbstractListModel>
#include <QHash>
#include <QMutex>

// Mir
#include <mir_toolkit/common.h>

// local
#include "mirsurfaceitem.h"

namespace mir { namespace scene { class Surface; class Session; }}

namespace qtmir {

class ShellServerConfiguration;

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
    void onSessionCreatedSurface(const mir::scene::Session *, const std::shared_ptr<mir::scene::Surface> &);
    void onSessionDestroyingSurface(const mir::scene::Session *, const std::shared_ptr<mir::scene::Surface> &);

    void onSurfaceAttributeChanged(const mir::scene::Surface *, MirSurfaceAttrib, int);

private:
    QHash<const mir::scene::Surface *, MirSurfaceItem *> m_mirSurfaceToItemHash;
    QList<MirSurfaceItem*> m_surfaceItems;
    ShellServerConfiguration* m_mirServer;
    static MirSurfaceManager *the_surface_manager;
    QHash<int, QByteArray> m_roleNames;
    QMutex m_mutex;
};

} // namespace qtmir

#endif // MIR_SURFACE_MANAGER_H
