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

//Qt
#include <QCoreApplication>

// mir
#include <mircommon/mir/geometry/rectangle.h>

// local
#include "inputarea.h"
#include "mirsurface.h"
#include "mirsurfacemanager.h"

// unity-mir
#include "mirserverconfiguration.h"
#include "logging.h"

InputArea::InputArea(QQuickItem *parent)
    : QQuickItem(parent)
    , m_enabled(true)
    , m_surface(nullptr)
{
    DLOG("InputArea::InputArea (this=%p)", this);
}

InputArea::~InputArea()
{
    DLOG("InputArea::~InputArea (this=%p)", this);
    if (m_surface) {
        m_surface->removeInputArea(this);
    }
}

bool InputArea::enabled() const
{
    return m_enabled;
}

void InputArea::setEnabled(bool enabled)
{
    DLOG("InputArea::setEnabled (this=%p, enabled=%p)", this, enabled ? "yes" : "no");

    if (enabled != m_enabled) {
        m_enabled = enabled;
        if (m_enabled) {
            setMirInputArea(m_geometry);
        }
        if (m_surface) {
            m_surface->enableInputArea(this, enabled);
        }
        Q_EMIT enabledChanged();
    }
}

MirSurface *InputArea::surface() const
{
    return m_surface;
}

void InputArea::setSurface(MirSurface *surface)
{
    doSetSurface(surface);
}

void InputArea::doSetSurface(MirSurface *surface)
{
    DLOG("InputArea::doSetSurface (this=%p, surface=%p)", this, surface);

    if (surface == m_surface) return;

    if (surface) {
        // If surface is destroyed, make sure this InputArea doesn't point to it any more
        // For now, the QML engine owns this object so the MirSurface cannot delete it
        connect(surface, &MirSurface::destroyed, [this]() { doSetSurface(nullptr); });
    }

    if (m_surface) {
        disconnect(surface, &MirSurface::destroyed, 0, 0);
        m_surface->removeInputArea(this);
    }

    m_surface = surface;

    if (m_enabled) {
        if (m_surface) {
            m_surface->installInputArea(this);
        }
    }
    Q_EMIT surfaceChanged();
}

void InputArea::geometryChanged(const QRectF& newGeometry, const QRectF& oldGeometry)
{
    DLOG("InputArea::geometryChanged (this=%p)", this);

    if (newGeometry != oldGeometry) {
        m_geometry = newGeometry;
        if (m_enabled) {
            setMirInputArea(m_geometry);
        }
    }
    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void InputArea::onAscendantChanged()
{
    DLOG("InputArea::onAscendantChanged (this=%p)", this);

    listenToAscendantsChanges();
    setMirInputArea(m_geometry);
}

void InputArea::onAscendantGeometryChanged()
{
    DLOG("InputArea::onAscendantGeometryChanged (this=%p)", this);

    setMirInputArea(m_geometry);
}

void InputArea::listenToAscendantsChanges()
{
    DLOG("InputArea::listenToAscendantsChanges (this=%p)", this);

    disconnectFromAscendantsChanges();

    /* FIXME: Listen to geometry changes and parent changes on all the ascendants.
       Listening on x, y, width, height changes on all the ascendants is necessary
       because there is no API that notifies of absolute geometry changes.
       Ref.: https://bugreports.qt-project.org/browse/QTBUG-19746
    */
    m_connections.append(connect(this, &QQuickItem::parentChanged, this, &InputArea::onAscendantChanged));
    QQuickItem* parent = parentItem();
    while (parent != NULL) {
        m_connections.append(connect(parent, &QQuickItem::parentChanged, this, &InputArea::onAscendantChanged));
        m_connections.append(connect(parent, &QQuickItem::xChanged, this, &InputArea::onAscendantGeometryChanged));
        m_connections.append(connect(parent, &QQuickItem::yChanged, this, &InputArea::onAscendantGeometryChanged));
        m_connections.append(connect(parent, &QQuickItem::widthChanged, this, &InputArea::onAscendantGeometryChanged));
        m_connections.append(connect(parent, &QQuickItem::heightChanged, this, &InputArea::onAscendantGeometryChanged));
        parent = parent->parentItem();
    }
}

void InputArea::disconnectFromAscendantsChanges()
{
    DLOG("InputArea::disconnectFromAscendantsChanges (this=%p)", this);

    // disconnect all previously connected signals
    Q_FOREACH (const QMetaObject::Connection& connection, m_connections) {
        disconnect(connection);
    }
    m_connections.clear();
}

void InputArea::setMirInputArea(const QRectF& relativeGeometry)
{
    DLOG("InputArea::setMirInputArea (this=%p, x=%lf, y=%lf, width=%lf, height=%lf)", this, relativeGeometry.x(), relativeGeometry.y(), relativeGeometry.width(), relativeGeometry.height());
    using namespace mir::geometry;
    const QRect rect = parentItem()->mapRectToScene(relativeGeometry).toRect();

    m_mirInputArea.top_left.x = X{rect.x()};
    m_mirInputArea.top_left.y = Y{rect.y()};
    m_mirInputArea.size.width = Width{rect.width()};
    m_mirInputArea.size.height = Height{rect.height()};

    if (m_surface) {
        m_surface->installInputArea(this);
    }
}
