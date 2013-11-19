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
#include "qsgmirsurfacenode.h"
#include "mirsurfaceitem.h"
#include "logging.h"

// Qt
#include <QSGSimpleRectNode>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QQmlEngine>
#include <QQuickWindow>

// Mir
#include <mir/shell/surface.h>
#include <mir/geometry/rectangle.h>

namespace mg = mir::geometry;

class QMirSurfaceTextureProvider : public QSGTextureProvider
{
    Q_OBJECT
public:
    QMirSurfaceTextureProvider() : t(0) { }
    ~QMirSurfaceTextureProvider() { delete t; }

    QSGTexture *texture() const {
        if (t)
            t->setFiltering(smooth ? QSGTexture::Linear : QSGTexture::Nearest);
        return t;
    }

    bool smooth;
    QSGTexture *t;

public Q_SLOTS:
    void invalidate()
    {
        delete t;
        t = 0;
    }
};

QMutex *MirSurfaceItem::mutex = 0;



MirSurfaceItem::MirSurfaceItem(std::shared_ptr<mir::shell::Surface> surface,
                               Application* application,
                               QQuickItem *parent)
    : QQuickItem(parent)
    , m_surface(surface)
    , m_application(application)
    , m_damaged(false)
    , m_surfaceValid(true)
    , m_provider(nullptr)
    , m_node(nullptr)
{
    DLOG("MirSurfaceItem::MirSurfaceItem");

    if (!mutex)
        mutex = new QMutex;

    setSmooth(true);
    setFlag(QQuickItem::ItemHasContents, true); //so scene graph will render this item
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton |
        Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::ExtraButton3 | Qt::ExtraButton4 |
        Qt::ExtraButton5 | Qt::ExtraButton6 | Qt::ExtraButton7 | Qt::ExtraButton8 |
        Qt::ExtraButton9 | Qt::ExtraButton10 | Qt::ExtraButton11 |
        Qt::ExtraButton12 | Qt::ExtraButton13);
    setAcceptHoverEvents(true);

    // Gift to QML engine. QML *must* delete this object to have Mir release the surface resources.
    QQmlEngine::setObjectOwnership(this, QQmlEngine::JavaScriptOwnership);
}

MirSurfaceItem::~MirSurfaceItem()
{
    QMutexLocker locker(mutex);
    if (m_node)
        m_node->setItem(0);
    if (m_provider)
        m_provider->deleteLater();
}

Application* MirSurfaceItem::application() const
{
    return m_application;
}

MirSurfaceItem::Type MirSurfaceItem::type() const
{
    return static_cast<MirSurfaceItem::Type>(m_surface->type());
}

MirSurfaceItem::State MirSurfaceItem::state() const
{
    return static_cast<MirSurfaceItem::State>(m_surface->state());
}

QString MirSurfaceItem::name() const
{
    //FIXME - how to listen to change in this property?
    return QString::fromStdString(m_surface->name());
}

QSGTextureProvider *MirSurfaceItem::textureProvider() const
{
    const_cast<MirSurfaceItem *>(this)->ensureProvider();
    return m_provider;
}

void MirSurfaceItem::ensureProvider()
{
    if (!m_provider) {
        m_provider = new QMirSurfaceTextureProvider();
        connect(window(), SIGNAL(sceneGraphInvalidated()),
                m_provider, SLOT(invalidate()), Qt::DirectConnection);
    }
}

void MirSurfaceItem::setDamagedFlag(bool on)
{
    m_damaged = on;
}

void MirSurfaceItem::surfaceDamaged(const QRect &)
{
    m_damaged = true;
    Q_EMIT textureChanged();
    update();
}

void MirSurfaceItem::updateTexture()    // called by render thread
{
    ensureProvider();
    QSGTexture *texture = m_provider->t;
    if (m_damaged) {
        m_damaged = false;
        QSGTexture *oldTexture = texture;
//        if (m_surface->type() == QWaylandSurface::Texture) {
//            QOpenGLContext *context = QOpenGLContext::currentContext();
//            QQuickWindow::CreateTextureOptions opt = 0;
//            if (useTextureAlpha()) {
//                opt |= QQuickWindow::TextureHasAlphaChannel;
//            }
//            texture = window()->createTextureFromId(m_surface->texture(context), m_surface->size(), opt);
//        } else {
//            texture = window()->createTextureFromImage(m_surface->image());
//        }
        texture->bind();
        delete oldTexture;
    }

    m_provider->t = texture;
    Q_EMIT m_provider->textureChanged();
    m_provider->smooth = smooth();
}

QSGNode *MirSurfaceItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)    // called by render thread
{
    if (!m_surface) {
        delete oldNode;
        return 0;
    }

    updateTexture();
    if (!m_provider->t) {
        delete oldNode;
        return 0;
    }

    QSGMirSurfaceNode *node = static_cast<QSGMirSurfaceNode *>(oldNode);

    if (!node) {
        node = new QSGMirSurfaceNode(this);
    }

    node->updateTexture();
    node->setRect(0, 0, width(), height());

    node->setTextureUpdated(true);

    return node;
}

void MirSurfaceItem::setSurfaceValid(const bool valid)
{
    if (valid != m_surfaceValid) {
        m_surfaceValid = valid;
        Q_EMIT surfaceValidChanged();
    }
}

qreal MirSurfaceItem::implicitHeight() const
{
    return static_cast<qreal>(m_surface->size().height.as_float());
}

qreal MirSurfaceItem::implicitWidth() const
{
    return static_cast<qreal>(m_surface->size().width.as_float());
}

void MirSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::mouseMoveEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::wheelEvent(QWheelEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::keyPressEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::keyReleaseEvent(QKeyEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::touchEvent(QTouchEvent *event)
{
    Q_UNUSED(event);
}

void MirSurfaceItem::setType(const Type &type)
{
    if (this->type() != type) {
        m_surface->configure(mir_surface_attrib_type, static_cast<int>(type));
    }
}

void MirSurfaceItem::setState(const State &state)
{
    if (this->state() != state) {
        m_surface->configure(mir_surface_attrib_state, static_cast<int>(state));
    }
}

// Called by MirSurfaceItemManager upon a msh::Surface attribute change
void MirSurfaceItem::setAttribute(const MirSurfaceAttrib attribute, const int /*value*/)
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

#include "mirsurfaceitem.moc"
