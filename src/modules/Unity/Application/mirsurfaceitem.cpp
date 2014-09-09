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
 *
 * Authors:
 *     Daniel d'Andrada <daniel.dandrada@canonical.com>
 *     Gerry Boland <gerry.boland@canonical.com>
 */

// local
#include "application.h"
#include "mirbuffersgtexture.h"
#include "session.h"
#include "mirsurfaceitem.h"
#include "logging.h"

// common
#include <debughelpers.h>

// Qt
#include <QDebug>
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QSGSimpleTextureNode>
#include <QSGTextureProvider>
#include <QTimer>

// Mir
#include <mir/geometry/rectangle.h>
#include <mir_toolkit/event.h>

namespace mg = mir::graphics;

namespace qtmir {

namespace {

bool fillInMirEvent(MirEvent &mirEvent, QKeyEvent *qtEvent)
{
    mirEvent.type = mir_event_type_key;

    // don't care
    mirEvent.key.device_id = 0;
    mirEvent.key.source_id = 0;

    switch (qtEvent->type()) {
        case QEvent::KeyPress:
            mirEvent.key.action = mir_key_action_down;
            break;
        case QEvent::KeyRelease:
            mirEvent.key.action = mir_key_action_up;
            break;
        default:
            return false;
    }

    // don't care
    mirEvent.key.flags = (MirKeyFlag)0;

    mirEvent.key.modifiers = qtEvent->nativeModifiers();
    mirEvent.key.key_code = qtEvent->nativeVirtualKey();
    mirEvent.key.scan_code = qtEvent->nativeScanCode();

    // TODO: Investigate how to pass it from mir to qt in the first place.
    //       Then implement the reverse here.
    mirEvent.key.repeat_count = 0;

    // Don't care
    mirEvent.key.down_time = 0;

    mirEvent.key.event_time = qtEvent->timestamp() * 1000000;

    // Don't care
    mirEvent.key.is_system_key = 0;

    return true;
}

bool fillInMirEvent(MirEvent &mirEvent, QTouchEvent *qtEvent)
{
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

    // Note: QtEventFeeder scales the event time down, scale it back up - precision is
    // lost but the time difference should still be accurate to milliseconds
    mirEvent.motion.event_time = static_cast<nsecs_t>(qtEvent->timestamp()) * 1000000;

    mirEvent.motion.pointer_count = qtEvent->touchPoints().count();

    auto touchPoints = qtEvent->touchPoints();
    for (int i = 0; i < touchPoints.count(); ++i) {
        auto touchPoint = touchPoints.at(i);
        auto &pointer = mirEvent.motion.pointer_coordinates[i];

        // FIXME: https://bugs.launchpad.net/mir/+bug/1311699
        // When multiple touch points are transmitted with a MirEvent
        // and one of them (only one is allowed) indicates a pressed
        // state change the index is encoded in the second byte of the
        // action value.
        const int mir_motion_event_pointer_index_shift = 8;
        if (mirEvent.motion.action == mir_motion_action_pointer_up &&
            touchPoint.state() == Qt::TouchPointReleased)
        {
            mirEvent.motion.action |= i << mir_motion_event_pointer_index_shift;
        }
        if (mirEvent.motion.action == mir_motion_action_pointer_down &&
            touchPoint.state() == Qt::TouchPointPressed)
        {
            mirEvent.motion.action |= i << mir_motion_event_pointer_index_shift;
        }


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

    return true;
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

void MirSurfaceObserver::frame_posted(int frames_available) {
    Q_UNUSED(frames_available);
    QMetaObject::invokeMethod(m_listener, "surfaceDamaged");
}

UbuntuKeyboardInfo *MirSurfaceItem::m_ubuntuKeyboardInfo = nullptr;

MirSurfaceItem::MirSurfaceItem(std::shared_ptr<mir::scene::Surface> surface,
                               QPointer<Session> session,
                               QQuickItem *parent)
    : QQuickItem(parent)
    , m_surface(surface)
    , m_session(session)
    , m_firstFrameDrawn(false)
    , m_live(true)
    , m_orientation(Qt::PortraitOrientation)
    , m_textureProvider(nullptr)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::MirSurfaceItem";

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
    // Rationale behind the frame dropper and its interval value:
    //
    // We want to give ample room for Qt scene graph to have a chance to fetch and render
    // the next pending buffer before we take the drastic action of dropping it (so don't set
    // it anywhere close to our target render interval).
    //
    // We also want to guarantee a minimal frames-per-second (fps) frequency for client applications
    // as they get stuck on swap_buffers() if there's no free buffer to swap to yet (ie, they
    // are all pending consumption by the compositor, us). But on the other hand, we don't want
    // that minimal fps to be too high as that would mean this timer would be triggered way too often
    // for nothing causing unnecessary overhead as actually dropping frames from an app should
    // in practice rarely happen.
    m_frameDropperTimer.setInterval(200);
    m_frameDropperTimer.setSingleShot(false);

    m_updateMirSurfaceSizeTimer.setSingleShot(true);
    m_updateMirSurfaceSizeTimer.setInterval(1);
    connect(&m_updateMirSurfaceSizeTimer, &QTimer::timeout, this, &MirSurfaceItem::updateMirSurfaceSize);
    connect(this, &QQuickItem::widthChanged, this, &MirSurfaceItem::scheduleMirSurfaceSizeUpdate);
    connect(this, &QQuickItem::heightChanged, this, &MirSurfaceItem::scheduleMirSurfaceSizeUpdate);

    // FIXME - setting surface unfocused immediately breaks camera & video apps, but is
    // technically the correct thing to do (surface should be unfocused until shell focuses it)
    //m_surface->configure(mir_surface_attrib_focus, mir_surface_unfocused);
    connect(this, &QQuickItem::activeFocusChanged, this, &MirSurfaceItem::updateMirSurfaceFocus);

    if (m_session) {
        connect(m_session.data(), &Session::stateChanged, this, &MirSurfaceItem::onSessionStateChanged);
    }
}

MirSurfaceItem::~MirSurfaceItem()
{
    if (m_session) {
        m_session->setSurface(nullptr);
    }

    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::~MirSurfaceItem - this=" << this;
    QMutexLocker locker(&m_mutex);
    m_surface->remove_observer(m_surfaceObserver);
    if (m_textureProvider)
        m_textureProvider->deleteLater();
}

// For QML to destroy this surface
void MirSurfaceItem::release()
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::release - this=" << this;

    if (m_session) {
        m_session->setSurface(nullptr);
    }
    deleteLater();
}

Session* MirSurfaceItem::session() const
{
    return m_session.data();
}

MirSurfaceItem::Type MirSurfaceItem::type() const
{
    return static_cast<MirSurfaceItem::Type>(m_surface->type());
}

MirSurfaceItem::State MirSurfaceItem::state() const
{
    return static_cast<MirSurfaceItem::State>(m_surface->state());
}

Qt::ScreenOrientation MirSurfaceItem::orientation() const
{
    return m_orientation;
}

void MirSurfaceItem::setOrientation(const Qt::ScreenOrientation orientation)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::setOrientation - orientation=" << orientation;

    if (m_orientation == orientation)
        return;

    MirOrientation mirOrientation;
    Qt::ScreenOrientation nativeOrientation = QGuiApplication::primaryScreen()->nativeOrientation();
    const bool landscapeNativeOrientation = (nativeOrientation == Qt::LandscapeOrientation);

    Qt::ScreenOrientation requestedOrientation = orientation;
    if (orientation == Qt::PrimaryOrientation) { // means orientation equals native orientation, set it as such
        requestedOrientation = nativeOrientation;
    }

    switch(requestedOrientation) {
    case Qt::PortraitOrientation:
        mirOrientation = (landscapeNativeOrientation) ? mir_orientation_right : mir_orientation_normal;
        break;
    case Qt::LandscapeOrientation:
        mirOrientation = (landscapeNativeOrientation) ? mir_orientation_normal : mir_orientation_left;
        break;
    case Qt::InvertedPortraitOrientation:
        mirOrientation = (landscapeNativeOrientation) ? mir_orientation_left : mir_orientation_inverted;
        break;
    case Qt::InvertedLandscapeOrientation:
        mirOrientation = (landscapeNativeOrientation) ? mir_orientation_inverted : mir_orientation_right;
        break;
    default:
        qWarning("Unrecognized Qt::ScreenOrientation!");
        return;
    }

    m_surface->set_orientation(mirOrientation);

    m_orientation = orientation;
    Q_EMIT orientationChanged();
}

QString MirSurfaceItem::name() const
{
    //FIXME - how to listen to change in this property?
    return QString::fromStdString(m_surface->name());
}

bool MirSurfaceItem::live() const
{
    return m_live;
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

    scheduleTextureUpdate();
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
    // TODO: Implement for desktop support
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

void MirSurfaceItem::keyPressEvent(QKeyEvent *qtEvent)
{
    MirEvent mirEvent;
    if (fillInMirEvent(mirEvent, qtEvent)) {
        m_surface->consume(mirEvent);
    }
}

void MirSurfaceItem::keyReleaseEvent(QKeyEvent *qtEvent)
{
    MirEvent mirEvent;
    if (fillInMirEvent(mirEvent, qtEvent)) {
        m_surface->consume(mirEvent);
    }
}

void MirSurfaceItem::touchEvent(QTouchEvent *event)
{
    MirEvent mirEvent;
    if (type() == InputMethod && event->type() == QEvent::TouchBegin) {
        // FIXME: Hack to get the VKB use case working while we don't have the proper solution in place.
        if (hasTouchInsideUbuntuKeyboard(event)) {
            if (fillInMirEvent(mirEvent, event)) {
                m_surface->consume(mirEvent);
            }
        } else {
            event->ignore();
        }

    } else {
        // NB: If we are getting QEvent::TouchUpdate or QEvent::TouchEnd it's because we've
        // previously accepted the corresponding QEvent::TouchBegin
        if (fillInMirEvent(mirEvent, event)) {
            m_surface->consume(mirEvent);
        }
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

void MirSurfaceItem::setLive(const bool live)
{
    if (m_live != live) {
        m_live = live;
        Q_EMIT liveChanged(m_live);
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

void MirSurfaceItem::scheduleMirSurfaceSizeUpdate()
{
    if (clientIsRunning() && !m_updateMirSurfaceSizeTimer.isActive()) {
        m_updateMirSurfaceSizeTimer.start();
    }
}

void MirSurfaceItem::updateMirSurfaceSize()
{
    int mirWidth = m_surface->size().width.as_int();
    int mirHeight = m_surface->size().height.as_int();

    int qmlWidth = (int)width();
    int qmlHeight = (int)height();

    bool mirSizeIsDifferent = qmlWidth != mirWidth || qmlHeight != mirHeight;

    const char *didResize = clientIsRunning() && mirSizeIsDifferent ? "surface resized" : "surface NOT resized";
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::updateMirSurfaceSize"
            << "surface =" << this
            << ", old (" << mirWidth << "," << mirHeight << ")"
            << ", new (" << qmlWidth << "," << qmlHeight << ")"
            << didResize;

    if (clientIsRunning() && mirSizeIsDifferent) {
        mir::geometry::Size newMirSize(qmlWidth, qmlHeight);
        m_surface->resize(newMirSize);
        setImplicitSize(qmlWidth, qmlHeight);
    }
}

void MirSurfaceItem::updateMirSurfaceFocus(bool focused)
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::updateMirSurfaceFocus" << focused;
    if (focused) {
        m_surface->configure(mir_surface_attrib_focus, mir_surface_focused);
    } else {
        m_surface->configure(mir_surface_attrib_focus, mir_surface_unfocused);
    }
}

void MirSurfaceItem::dropPendingBuffers()
{
    QMutexLocker locker(&m_mutex);

    std::unique_ptr<mg::Renderable> renderable =
        m_surface->compositor_snapshot((void*)123/*user_id*/);

    while (renderable->buffers_ready_for_compositor() > 0) {
        // The line below looks like an innocent, effect-less, getter. But as this
        // method returns a unique_pointer, not holding its reference causes the
        // buffer to be destroyed/released straight away.
        m_surface->compositor_snapshot((void*)123/*user_id*/)->buffer();
        qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::dropPendingBuffers()"
            << "surface =" << this
            << "buffer dropped."
            << renderable->buffers_ready_for_compositor()
            << "left.";
    }
}

void MirSurfaceItem::stopFrameDropper()
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::stopFrameDropper surface = " << this;
    QMutexLocker locker(&m_mutex);
    m_frameDropperTimer.stop();
}

void MirSurfaceItem::startFrameDropper()
{
    qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::startFrameDropper surface = " << this;
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

void MirSurfaceItem::setSession(Session *session)
{
    m_session = session;
}

void MirSurfaceItem::onSessionStateChanged(Session::State state)
{
    switch (state) {
        case Session::State::Running:
            syncSurfaceSizeWithItemSize();
            break;
        default:
            break;
    }
}

void MirSurfaceItem::syncSurfaceSizeWithItemSize()
{
    int mirWidth = m_surface->size().width.as_int();
    int mirHeight = m_surface->size().width.as_int();

    if ((int)width() != mirWidth || (int)height() != mirHeight) {
        qCDebug(QTMIR_SURFACES) << "MirSurfaceItem::syncSurfaceSizeWithItemSize()";
        mir::geometry::Size newMirSize((int)width(), (int)height());
        m_surface->resize(newMirSize);
        setImplicitSize(width(), height());
    }
}

bool MirSurfaceItem::clientIsRunning() const
{
    return (m_session &&
            (m_session->state() == Session::State::Running
             || m_session->state() == Session::State::Starting))
        || !m_session;
}

} // namespace qtmir

#include "mirsurfaceitem.moc"
