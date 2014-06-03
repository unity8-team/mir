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
 *
 * Authors:
 *     Daniel d'Andrada <daniel.dandrada@canonical.com>
 *     Gerry Boland <gerry.boland@canonical.com>
 *
 * Bits and pieces taken from the QtWayland portion of the Qt project which is
 * Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
 */

// local
#include "application.h"
#include "debughelpers.h"
#include "mirbuffersgtexture.h"
#include "mirsurfaceitem.h"
#include "logging.h"

// Qt
#include <QDebug>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QSGTextureProvider>
#include <QTimer>

// Mir
#include <mir/geometry/rectangle.h>
#include <mir_toolkit/event.h>

namespace mg = mir::graphics;

namespace {

/* simplified version of systemTime from the android project, with the following copyright */
/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
nsecs_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return nsecs_t(t.tv_sec)*1000000000LL + t.tv_nsec;
}

MirEvent createMirEvent(QTouchEvent *qtEvent)
{
    MirEvent mirEvent;

    mirEvent.type = mir_event_type_motion;

    // Hardcoding it for now
    // TODO: Gather this info from a QTouchDevice-derived class created by QtEventFeeder
    mirEvent.motion.device_id = 0; 
    mirEvent.motion.source_id = 0x00001002; // AINPUT_SOURCE_TOUCHSCREEN; https://bugs.launchpad.net/bugs/1311687

    // NB: it's assumed that touch points are pressed and released
    // one at a time.

    if (qtEvent->touchPointStates().testFlag(Qt::TouchPointPressed)) {
        if (qtEvent->touchPoints().count() > 1) {
            mirEvent.motion.action = mir_motion_action_pointer_down;
        } else {
            mirEvent.motion.action = mir_motion_action_down;
        }
    } else if (qtEvent->touchPointStates().testFlag(Qt::TouchPointReleased)) {
        if (qtEvent->touchPoints().count() > 1) {
            mirEvent.motion.action = mir_motion_action_pointer_up;
        } else {
            mirEvent.motion.action = mir_motion_action_up;
        }
    } else {
            mirEvent.motion.action = mir_motion_action_move;
    }

    // not used
    mirEvent.motion.flags = (MirMotionFlag) 0;

    // TODO: map QInputEvent::modifiers()
    mirEvent.motion.modifiers = 0;

    // not used
    mirEvent.motion.edge_flags = 0;

    // TODO
    mirEvent.motion.button_state = (MirMotionButton) 0;

    // Does it matter?
    mirEvent.motion.x_offset = 0.;
    mirEvent.motion.y_offset = 0.;
    mirEvent.motion.x_precision = 0.1;
    mirEvent.motion.y_precision = 0.1;

    // TODO. Not useful to Qt at least...
    mirEvent.motion.down_time = 0;

    // Using qtEvent->timestamp() didn't work (don't remember why)
    mirEvent.motion.event_time = systemTime();

    mirEvent.motion.pointer_count = qtEvent->touchPoints().count();

    auto touchPoints = qtEvent->touchPoints();
    for (int i = 0; i < touchPoints.count(); ++i) {
        auto touchPoint = touchPoints.at(i);
        auto &pointer = mirEvent.motion.pointer_coordinates[i];

        pointer.id = touchPoint.id();
        pointer.x = touchPoint.pos().x();
        pointer.y = touchPoint.pos().y();

        // FIXME: https://bugs.launchpad.net/mir/+bug/1311809

        if (touchPoint.rawScreenPositions().isEmpty()) {
            pointer.raw_x = 0.;
            pointer.raw_y = 0.;
        } else {
            pointer.raw_x = touchPoint.rawScreenPositions().at(0).x();
            pointer.raw_y =  touchPoint.rawScreenPositions().at(0).y();
        }

        pointer.touch_major = 0.;
        pointer.touch_minor = 0.;
        pointer.size = 0.;
        pointer.pressure = touchPoint.pressure();
        pointer.orientation = 0.;
        pointer.vscroll = 0.;
        pointer.hscroll = 0.;
        pointer.tool_type = mir_motion_tool_type_unknown;
    }

    return mirEvent;
}

} // namespace {

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
    MirBufferSGTexture *t;

public Q_SLOTS:
    void invalidate()
    {
        delete t;
        t = 0;
    }
};

MirSurfaceObserver::MirSurfaceObserver()
    : m_listener(nullptr) {
}

void MirSurfaceObserver::setListener(QObject *listener) {
    m_listener = listener;
}

void MirSurfaceObserver::frame_posted() {
    QMetaObject::invokeMethod(m_listener, "surfaceDamaged");
}

UbuntuKeyboardInfo *MirSurfaceItem::m_ubuntuKeyboardInfo = nullptr;

MirSurfaceItem::MirSurfaceItem(std::shared_ptr<mir::scene::Surface> surface,
                               QQuickItem *parent)
    : QQuickItem(parent)
    , m_surface(surface)
    , m_firstFrameDrawn(false)
    , m_textureProvider(nullptr)
{
    DLOG("MirSurfaceItem::MirSurfaceItem");

    m_surfaceObserver = std::make_shared<MirSurfaceObserver>();
    m_surfaceObserver->setListener(this);
    m_surface->add_observer(m_surfaceObserver);

    setSmooth(true);
    setFlag(QQuickItem::ItemHasContents, true); //so scene graph will render this item
    setAcceptedMouseButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton |
        Qt::ExtraButton1 | Qt::ExtraButton2 | Qt::ExtraButton3 | Qt::ExtraButton4 |
        Qt::ExtraButton5 | Qt::ExtraButton6 | Qt::ExtraButton7 | Qt::ExtraButton8 |
        Qt::ExtraButton9 | Qt::ExtraButton10 | Qt::ExtraButton11 |
        Qt::ExtraButton12 | Qt::ExtraButton13);
    setAcceptHoverEvents(true);

    // fetch surface geometry
    setImplicitSize(static_cast<qreal>(m_surface->size().width.as_float()),
                    static_cast<qreal>(m_surface->size().height.as_float()));

    if (!m_ubuntuKeyboardInfo) {
        m_ubuntuKeyboardInfo = new UbuntuKeyboardInfo;
    }

    // Ensure C++ (MirSurfaceManager) retains ownership of this object
    // TODO: Investigate if having the Javascript engine have ownership of this object
    // might create a less error-prone API design (concern: QML forgets to call "release()"
    // for a surface, and thus Mir will not release the surface buffers etc.)
    QQmlEngine::setObjectOwnership(this, QQmlEngine::CppOwnership);

    connect(&m_frameDropperTimer, &QTimer::timeout,
            this, &MirSurfaceItem::dropPendingBuffers);
    m_frameDropperTimer.setInterval(200);
    m_frameDropperTimer.setSingleShot(false);
}

MirSurfaceItem::~MirSurfaceItem()
{
    DLOG("MirSurfaceItem::~MirSurfaceItem(this=%p)", this);
    m_surface->remove_observer(m_surfaceObserver);
    if (m_textureProvider)
        m_textureProvider->deleteLater();
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

// Called from the rendering (scene graph) thread
QSGTextureProvider *MirSurfaceItem::textureProvider() const
{
    const_cast<MirSurfaceItem *>(this)->ensureProvider();
    return m_textureProvider;
}

void MirSurfaceItem::ensureProvider()
{
    if (!m_textureProvider) {
        m_textureProvider = new QMirSurfaceTextureProvider();
        connect(window(), SIGNAL(sceneGraphInvalidated()),
                m_textureProvider, SLOT(invalidate()), Qt::DirectConnection);
    }
}

void MirSurfaceItem::surfaceDamaged()
{
    if (!m_firstFrameDrawn) {
        m_firstFrameDrawn = true;
        Q_EMIT firstFrameDrawn(this);
    }

    if (m_application &&
            (m_application->state() == Application::Running
            || m_application->state() == Application::Starting)) {
        scheduleTextureUpdate();
    }
}

bool MirSurfaceItem::updateTexture()    // called by rendering thread (scene graph)
{
    QMutexLocker locker(&m_mutex);
    ensureProvider();
    bool textureUpdated = false;

    std::unique_ptr<mg::Renderable> renderable =
        m_surface->compositor_snapshot((void*)123/*user_id*/);

    if (renderable->buffers_ready_for_compositor() > 0) {
        if (!m_textureProvider->t) {
            m_textureProvider->t = new MirBufferSGTexture(renderable->buffer());
        } else {
            // Avoid holding two buffers for the compositor at the same time. Thus free the current
            // before acquiring the next
            m_textureProvider->t->freeBuffer();
            m_textureProvider->t->setBuffer(renderable->buffer());
        }
        textureUpdated = true;
    }

    if (renderable->buffers_ready_for_compositor() > 0) {
        QTimer::singleShot(0, this, SLOT(update()));
        // restart the frame dropper so that we have enough time to render the next frame.
        m_frameDropperTimer.start();
    }

    m_textureProvider->smooth = smooth();

    return textureUpdated;
}

QSGNode *MirSurfaceItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)    // called by render thread
{
    if (!m_surface) {
        delete oldNode;
        return 0;
    }

    bool textureUpdated = updateTexture();
    if (!m_textureProvider->t) {
        delete oldNode;
        return 0;
    }

    QSGSimpleTextureNode *node = static_cast<QSGSimpleTextureNode*>(oldNode);
    if (!node) {
        node = new QSGSimpleTextureNode;
        node->setTexture(m_textureProvider->t);
    } else {
        if (textureUpdated) {
            node->markDirty(QSGNode::DirtyMaterial);
        }
    }

    node->setRect(0, 0, width(), height());

    return node;
}

void MirSurfaceItem::mousePressEvent(QMouseEvent *event)
{
    // we don't care about them for now
    event->ignore();
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
    if (type() == InputMethod && event->type() == QEvent::TouchBegin) {
        // FIXME: Hack to get the VKB use case working while we don't have the proper solution in place.
        if (hasTouchInsideUbuntuKeyboard(event)) {
            MirEvent mirEvent = createMirEvent(event);
            m_surface->consume(mirEvent);
        } else {
            event->ignore();
        }

    } else {
        // NB: If we are getting QEvent::TouchUpdate or QEvent::TouchEnd it's because we've
        // previously accepted the corresponding QEvent::TouchBegin
        MirEvent mirEvent = createMirEvent(event);
        m_surface->consume(mirEvent);
    }
}

bool MirSurfaceItem::hasTouchInsideUbuntuKeyboard(QTouchEvent *event)
{
    const QList<QTouchEvent::TouchPoint> &touchPoints = event->touchPoints();
    for (int i = 0; i < touchPoints.count(); ++i) {
        QPoint pos = touchPoints.at(i).pos().toPoint();
        if (pos.x() >= m_ubuntuKeyboardInfo->x()
                && pos.x() <= (m_ubuntuKeyboardInfo->x() + m_ubuntuKeyboardInfo->width())
                && pos.y() >= m_ubuntuKeyboardInfo->y()
                && pos.y() <= (m_ubuntuKeyboardInfo->y() + m_ubuntuKeyboardInfo->height())) {
            return true;
        }
    }
    return false;
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

void MirSurfaceItem::geometryChanged(const QRectF &newGeometry, const QRectF &oldGeometry)
{

    int mirWidth = m_surface->size().width.as_int();
    int mirHeight = m_surface->size().width.as_int();

    if (m_application &&
            (m_application->state() == Application::Running
             || m_application->state() == Application::Starting)
            && ((int)newGeometry.width() != mirWidth
                || (int)newGeometry.height() != mirHeight)) {

        qDebug() << "MirSurfaceItem::geometryChanged"
                << "appId =" << appId()
                << ", oldGeometry" << oldGeometry
                << ", newGeometry" << newGeometry
                << "surface resized";

        mir::geometry::Size newMirSize((int)newGeometry.width(), (int)newGeometry.height());
        m_surface->resize(newMirSize);
        setImplicitSize(newGeometry.width(), newGeometry.height());
    } else {
        qDebug() << "MirSurfaceItem::geometryChanged"
                << "appId =" << appId()
                << ", oldGeometry" << oldGeometry
                << ", newGeometry" << newGeometry
                << "surface NOT resized";
    }

    QQuickItem::geometryChanged(newGeometry, oldGeometry);
}

void MirSurfaceItem::dropPendingBuffers()
{
    QMutexLocker locker(&m_mutex);

    std::unique_ptr<mg::Renderable> renderable =
        m_surface->compositor_snapshot((void*)123/*user_id*/);

    while (renderable->buffers_ready_for_compositor() > 0) {
        m_surface->compositor_snapshot((void*)123/*user_id*/)->buffer();
        qDebug() << "MirSurfaceItem::dropPendingBuffers()"
            << "appId =" << appId()
            << "buffer dropped."
            << renderable->buffers_ready_for_compositor()
            << "left.";
    }
}

void MirSurfaceItem::stopFrameDropper()
{
    qDebug() << "MirSurfaceItem::stopFrameDropper appId = " << appId();
    QMutexLocker locker(&m_mutex);
    m_frameDropperTimer.stop();
}

void MirSurfaceItem::startFrameDropper()
{
    qDebug() << "MirSurfaceItem::startFrameDropper appId = " << appId();
    QMutexLocker locker(&m_mutex);
    if (!m_frameDropperTimer.isActive()) {
        m_frameDropperTimer.start();
    }
}

void MirSurfaceItem::scheduleTextureUpdate()
{
    QMutexLocker locker(&m_mutex);

    // Notify QML engine that this needs redrawing, schedules call to updatePaintItem
    update();
    // restart the frame dropper so that we have enough time to render the next frame.
    m_frameDropperTimer.start();
}

QString MirSurfaceItem::appId()
{
    if (m_application) {
        return m_application->appId();
    } else {
        return QString();
    }
}

void MirSurfaceItem::setApplication(Application *app)
{
    m_application = app;
}

void MirSurfaceItem::onApplicationStateChanged()
{
    if (m_application->state() == Application::Running) {
        syncSurfaceSizeWithItemSize();
    }
}

void MirSurfaceItem::syncSurfaceSizeWithItemSize()
{
    int mirWidth = m_surface->size().width.as_int();
    int mirHeight = m_surface->size().width.as_int();

    if ((int)width() != mirWidth || (int)height() != mirHeight) {
        qDebug("MirSurfaceItem::syncSurfaceSizeWithItemSize()");
        mir::geometry::Size newMirSize((int)width(), (int)height());
        m_surface->resize(newMirSize);
        setImplicitSize(width(), height());
    }
}

#include "mirsurfaceitem.moc"
