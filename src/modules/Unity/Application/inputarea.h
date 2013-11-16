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

#ifndef INPUTAREA_H
#define INPUTAREA_H

// Qt
#include <QtQuick/QQuickItem>
#include <QtCore/QLinkedList>
#include <QtCore/QMetaObject>

// mir
#include <mircommon/mir/geometry/rectangle.h>

class MirSurface;
namespace mir { namespace shell { class Surface; }}
class InputArea : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(MirSurface* surface READ surface WRITE setSurface NOTIFY surfaceChanged)

public:
    explicit InputArea(QQuickItem *parent = 0);
    ~InputArea();

    bool enabled() const;
    MirSurface *surface() const;

    void setEnabled(bool enabled);
    virtual void setSurface(MirSurface *surface);

Q_SIGNALS:
    void enabledChanged();
    void surfaceChanged();

protected:
    virtual void geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry);
    void doSetSurface(MirSurface *surface);

private Q_SLOTS:
    void onAscendantChanged();
    void onAscendantGeometryChanged();

private:
    void listenToAscendantsChanges();
    void disconnectFromAscendantsChanges();
    void setMirInputArea(const QRectF& relativeGeometry);

    bool m_enabled;
    QRectF m_geometry;
    MirSurface* m_surface;
    mir::geometry::Rectangle m_mirInputArea;
    QLinkedList<QMetaObject::Connection> m_connections;

    friend class MirSurface;
};

#endif // INPUTAREA_H
