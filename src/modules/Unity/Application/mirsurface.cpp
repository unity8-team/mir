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

// local
#include "mirsurface.h"
#include "inputarea.h"

// unity-mir
#include "logging.h"

// Mir
#include <mir/shell/surface.h>
#include <mir/geometry/rectangle.h>

namespace mg = mir::geometry;

MirSurface::MirSurface(std::shared_ptr<mir::shell::Surface> surface, Application* application, QQuickItem *parent)
    : QQuickItem(parent)
    , m_surface(surface)
    , m_application(application)
{
    DLOG("MirSurface::MirSurface");
    setFlag(QQuickItem::ItemHasContents, false); //so scene graph will not render this item
}

MirSurface::~MirSurface()
{
    m_inputAreas.clear();
}

Application* MirSurface::application() const
{
    return m_application;
}

qreal MirSurface::x() const
{
    int xAbsolute = m_surface->top_left().x.as_int();
    return mapFromScene((QPointF(xAbsolute, 0))).x();
}

qreal MirSurface::y() const
{
    int yAbsolute = m_surface->top_left().y.as_int();
    return mapFromScene((QPointF(0, yAbsolute))).y();
}

void MirSurface::setX(qreal xValue)
{
    using namespace mir::geometry;
    qreal xAbsolute = mapToScene(QPointF(xValue, 0)).x();

    Point position = m_surface->top_left();
    if (position.x.as_int() != (int) xAbsolute) {
        position.x = X{xAbsolute};
        m_surface->move_to(position);
        Q_EMIT xChanged();
    }
}

void MirSurface::setY(qreal yValue)
{
    using namespace mir::geometry;
    qreal yAbsolute = mapToScene(QPointF(0, yValue)).y();

    Point position = m_surface->top_left();
    if (position.y.as_int() != (int) yAbsolute) {
        position.y = Y{yAbsolute};
        m_surface->move_to(position);
        Q_EMIT yChanged();
    }
}

qreal MirSurface::width() const
{
    return m_surface->size().width.as_int();
}

qreal MirSurface::height() const
{
    return m_surface->size().height.as_int();
}

void MirSurface::setWidth(qreal widthValue)
{
    using namespace mir::geometry;

    Size size = m_surface->size();
    if (size.width.as_int() != (int) widthValue) {
        size.width = Width{widthValue};
//        m_surface->set_size(size); //MISSING FROM API
        Q_EMIT widthChanged();
    }
}

void MirSurface::setHeight(qreal heightValue)
{
    using namespace mir::geometry;

    Size size = m_surface->size();
    if (size.height.as_int() != (int) heightValue) {
        size.height = Height{heightValue};
//        m_surface->set_size(size); //MISSING FROM API
        Q_EMIT heightChanged();
    }
}

bool MirSurface::isVisible() const
{
    return m_visible;
}

void MirSurface::setVisible(const bool visible)
{
    if (visible == m_visible) return;

    if (visible) {
        m_surface->show();
    } else {
        m_surface->hide();
    }
    m_visible = visible;
    Q_EMIT visibleChanged();
}

MirSurface::Type MirSurface::type() const
{
    return static_cast<MirSurface::Type>(m_surface->type());
}

MirSurface::State MirSurface::state() const
{
    return static_cast<MirSurface::State>(m_surface->state());
}

QString MirSurface::name() const
{
    //FIXME - how to listen to change in this property
    return QString::fromStdString(m_surface->name());
}

void MirSurface::installInputArea(const InputArea* area)
{
    if (!m_surface->supports_input()) {
        LOG("MirSurface::installInputArea - surface does not support input");
    }

    m_inputAreas.insert(area);
    updateMirInputRegion();
}

bool MirSurface::removeInputArea(const InputArea* area)
{
    const bool res = m_inputAreas.remove(area);
    updateMirInputRegion();
    return res;
}

bool MirSurface::enableInputArea(const InputArea* area, bool enable)
{
    bool res;
    if (enable) {
        m_inputAreas.insert(area);
        res = true;
    } else {
        res = m_inputAreas.remove(area);
    }
    updateMirInputRegion();
    return res;
}

void MirSurface::updateMirInputRegion()
{
    /* WARNING: by default, a surface has an input region covering the whole surface.
       Once the surface input region is set, the default will *not* be restored when
       all input regions are removed/disabled */
    std::vector<mir::geometry::Rectangle> mirInputAreas;
    for (auto const& area : m_inputAreas) {
        mirInputAreas.push_back(area->m_mirInputArea);
    }
    m_surface->set_input_region(mirInputAreas);
}

void MirSurface::setType(const Type &type)
{
    if (this->type() != type) {
        m_surface->configure(mir_surface_attrib_type, static_cast<int>(type));
    }
}

void MirSurface::setState(const State &state)
{
    if (this->state() != state) {
        m_surface->configure(mir_surface_attrib_state, static_cast<int>(state));
    }
}

// Called by MirSurfaceManager upon a msh::Surface attribute change
void MirSurface::setAttribute(const MirSurfaceAttrib attribute, const int /*value*/)
{
    switch (attribute) {
    case mir_surface_attrib_type:
        Q_EMIT typeChanged();
        break;
    case mir_surface_attrib_state:
        Q_EMIT stateChanged();
        break;
    default:
        break;
    }
}

