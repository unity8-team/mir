/*
 * Copyright © 2013-2014 Canonical Ltd.
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
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 *              Gerry Boland <gerry.boland@canonical.com>
 */

#include "qteventfeeder.h"
#include "logging.h"

#include <qpa/qplatforminputcontext.h>
#include <qpa/qplatformintegration.h>
#include <QGuiApplication>
#include <private/qguiapplication_p.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <QDebug>

Q_LOGGING_CATEGORY(QTMIR_MIR_INPUT, "qtmir.mir.input")

// from android-input AMOTION_EVENT_ACTION_*, hidden inside mir bowels
// mir headers should define them
const int QtEventFeeder::MirEventActionMask = 0xff;
const int QtEventFeeder::MirEventActionPointerIndexMask = 0xff00;
const int QtEventFeeder::MirEventActionPointerIndexShift = 8;


// XKB Keysyms which do not map directly to Qt types (i.e. Unicode points)
static const uint32_t KeyTable[] = {
    XKB_KEY_Escape,                  Qt::Key_Escape,
    XKB_KEY_Tab,                     Qt::Key_Tab,
    XKB_KEY_ISO_Left_Tab,            Qt::Key_Backtab,
    XKB_KEY_BackSpace,               Qt::Key_Backspace,
    XKB_KEY_Return,                  Qt::Key_Return,
    XKB_KEY_Insert,                  Qt::Key_Insert,
    XKB_KEY_Delete,                  Qt::Key_Delete,
    XKB_KEY_Clear,                   Qt::Key_Delete,
    XKB_KEY_Pause,                   Qt::Key_Pause,
    XKB_KEY_Print,                   Qt::Key_Print,

    XKB_KEY_Home,                    Qt::Key_Home,
    XKB_KEY_End,                     Qt::Key_End,
    XKB_KEY_Left,                    Qt::Key_Left,
    XKB_KEY_Up,                      Qt::Key_Up,
    XKB_KEY_Right,                   Qt::Key_Right,
    XKB_KEY_Down,                    Qt::Key_Down,
    XKB_KEY_Prior,                   Qt::Key_PageUp,
    XKB_KEY_Next,                    Qt::Key_PageDown,

    XKB_KEY_Shift_L,                 Qt::Key_Shift,
    XKB_KEY_Shift_R,                 Qt::Key_Shift,
    XKB_KEY_Shift_Lock,              Qt::Key_Shift,
    XKB_KEY_Control_L,               Qt::Key_Control,
    XKB_KEY_Control_R,               Qt::Key_Control,
    XKB_KEY_Meta_L,                  Qt::Key_Meta,
    XKB_KEY_Meta_R,                  Qt::Key_Meta,
    XKB_KEY_Alt_L,                   Qt::Key_Alt,
    XKB_KEY_Alt_R,                   Qt::Key_Alt,
    XKB_KEY_Caps_Lock,               Qt::Key_CapsLock,
    XKB_KEY_Num_Lock,                Qt::Key_NumLock,
    XKB_KEY_Scroll_Lock,             Qt::Key_ScrollLock,
    XKB_KEY_Super_L,                 Qt::Key_Super_L,
    XKB_KEY_Super_R,                 Qt::Key_Super_R,
    XKB_KEY_Menu,                    Qt::Key_Menu,
    XKB_KEY_Hyper_L,                 Qt::Key_Hyper_L,
    XKB_KEY_Hyper_R,                 Qt::Key_Hyper_R,
    XKB_KEY_Help,                    Qt::Key_Help,

    XKB_KEY_KP_Space,                Qt::Key_Space,
    XKB_KEY_KP_Tab,                  Qt::Key_Tab,
    XKB_KEY_KP_Enter,                Qt::Key_Enter,
    XKB_KEY_KP_Home,                 Qt::Key_Home,
    XKB_KEY_KP_Left,                 Qt::Key_Left,
    XKB_KEY_KP_Up,                   Qt::Key_Up,
    XKB_KEY_KP_Right,                Qt::Key_Right,
    XKB_KEY_KP_Down,                 Qt::Key_Down,
    XKB_KEY_KP_Prior,                Qt::Key_PageUp,
    XKB_KEY_KP_Next,                 Qt::Key_PageDown,
    XKB_KEY_KP_End,                  Qt::Key_End,
    XKB_KEY_KP_Begin,                Qt::Key_Clear,
    XKB_KEY_KP_Insert,               Qt::Key_Insert,
    XKB_KEY_KP_Delete,               Qt::Key_Delete,
    XKB_KEY_KP_Equal,                Qt::Key_Equal,
    XKB_KEY_KP_Multiply,             Qt::Key_Asterisk,
    XKB_KEY_KP_Add,                  Qt::Key_Plus,
    XKB_KEY_KP_Separator,            Qt::Key_Comma,
    XKB_KEY_KP_Subtract,             Qt::Key_Minus,
    XKB_KEY_KP_Decimal,              Qt::Key_Period,
    XKB_KEY_KP_Divide,               Qt::Key_Slash,

    XKB_KEY_ISO_Level3_Shift,        Qt::Key_AltGr,
    XKB_KEY_Multi_key,               Qt::Key_Multi_key,
    XKB_KEY_Codeinput,               Qt::Key_Codeinput,
    XKB_KEY_SingleCandidate,         Qt::Key_SingleCandidate,
    XKB_KEY_MultipleCandidate,       Qt::Key_MultipleCandidate,
    XKB_KEY_PreviousCandidate,       Qt::Key_PreviousCandidate,

    XKB_KEY_Mode_switch,             Qt::Key_Mode_switch,
    XKB_KEY_script_switch,           Qt::Key_Mode_switch,
    XKB_KEY_XF86AudioRaiseVolume,    Qt::Key_VolumeUp,
    XKB_KEY_XF86AudioLowerVolume,    Qt::Key_VolumeDown,
    XKB_KEY_XF86PowerOff,            Qt::Key_PowerOff,
    XKB_KEY_XF86PowerDown,           Qt::Key_PowerDown,

    0,                          0
};

static uint32_t translateKeysym(uint32_t sym, char *string, size_t size) {
    Q_UNUSED(size);
    string[0] = '\0';

    if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F35)
        return Qt::Key_F1 + (int(sym) - XKB_KEY_F1);

    for (int i = 0; KeyTable[i]; i += 2)
        if (sym == KeyTable[i])
            return KeyTable[i + 1];

    string[0] = sym;
    string[1] = '\0';
    return toupper(sym);
}

namespace {

class QtWindowSystem : public QtEventFeeder::QtWindowSystemInterface {

    bool hasTargetWindow() override
    {
        if (mTopLevelWindow.isNull() && !QGuiApplication::topLevelWindows().isEmpty()) {
            mTopLevelWindow = QGuiApplication::topLevelWindows().first();
        }
        return !mTopLevelWindow.isNull();
    }

    QRect targetWindowGeometry() override
    {
        Q_ASSERT(!mTopLevelWindow.isNull());
        return mTopLevelWindow->geometry();
    }

    void registerTouchDevice(QTouchDevice *device) override
    {
        QWindowSystemInterface::registerTouchDevice(device);
    }

    void handleExtendedKeyEvent(ulong timestamp, QEvent::Type type, int key,
                Qt::KeyboardModifiers modifiers,
                quint32 nativeScanCode, quint32 nativeVirtualKey,
                quint32 nativeModifiers,
                const QString& text, bool autorep, ushort count) override
    {
        Q_ASSERT(!mTopLevelWindow.isNull());
        QWindowSystemInterface::handleExtendedKeyEvent(mTopLevelWindow.data(), timestamp, type, key, modifiers,
                nativeScanCode, nativeVirtualKey, nativeModifiers, text, autorep, count);
    }

    void handleTouchEvent(ulong timestamp, QTouchDevice *device,
            const QList<struct QWindowSystemInterface::TouchPoint> &points, Qt::KeyboardModifiers mods) override
    {
        Q_ASSERT(!mTopLevelWindow.isNull());
        QWindowSystemInterface::handleTouchEvent(mTopLevelWindow.data(), timestamp, device, points, mods);
    }
private:
    QPointer<QWindow> mTopLevelWindow;
};

} // anonymous namespace


QtEventFeeder::QtEventFeeder(QtEventFeeder::QtWindowSystemInterface *windowSystem)
{
    if (windowSystem) {
        mQtWindowSystem = windowSystem;
    } else {
        mQtWindowSystem = new QtWindowSystem;
    }

    // Initialize touch device. Hardcoded just like in qtubuntu
    // TODO: Create them from info gathered from Mir and store things like device id and source
    //       in a QTouchDevice-derived class created by us. So that we can properly assemble back
    //       MirEvents our of QTouchEvents to give to mir::scene::Surface::consume.
    mTouchDevice = new QTouchDevice();  // Qt takes ownership of mTouchDevice with registerTouchDevice
    mTouchDevice->setType(QTouchDevice::TouchScreen);
    mTouchDevice->setCapabilities(
            QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure |
            QTouchDevice::NormalizedPosition);
    mQtWindowSystem->registerTouchDevice(mTouchDevice);
}

QtEventFeeder::~QtEventFeeder()
{
    delete mQtWindowSystem;
}

void QtEventFeeder::dispatch(MirEvent const& event)
{
    switch (event.type) {
    case mir_event_type_key:
        dispatchKey(event.key);
        break;
    case mir_event_type_motion:
        dispatchMotion(event.motion);
        break;
    case mir_event_type_surface:
        // Just ignore these events: it doesn't make sense to pass them on.
        break;
    default:
        // mir_event_type_surface and mir_event_type_resize events go through
        // mir's own protobuf channel instead of the android_input one. The latter
        // being the one we're dealing with here.
        qFatal("QtEventFeeder got unsupported event type from mir");
        break;
    }
}

void QtEventFeeder::dispatchKey(MirKeyEvent const& event)
{
    if (!mQtWindowSystem->hasTargetWindow())
        return;

    ulong timestamp = event.event_time / 1000000;
    xkb_keysym_t xk_sym = static_cast<xkb_keysym_t>(event.key_code);

    // Key modifier and unicode index mapping.
    const int kEventModifiers = event.modifiers;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    if (kEventModifiers & mir_key_modifier_shift) {
        modifiers |= Qt::ShiftModifier;
    }
    if (kEventModifiers & mir_key_modifier_ctrl) {
        modifiers |= Qt::ControlModifier;
    }
    if (kEventModifiers & mir_key_modifier_alt) {
        modifiers |= Qt::AltModifier;
    }
    if (kEventModifiers & mir_key_modifier_meta) {
        modifiers |= Qt::MetaModifier;
    }

    // Key action
    QEvent::Type keyType;
    if (event.action == mir_key_action_down) {
        keyType = QEvent::KeyPress;
    } else {
        keyType = QEvent::KeyRelease;
    }

    // Key event propagation.
    char s[2];
    int keyCode = translateKeysym(xk_sym, s, sizeof(s));
    QString text = QString::fromLatin1(s);

    QPlatformInputContext* context = QGuiApplicationPrivate::platformIntegration()->inputContext();
    if (context) {
        // TODO: consider event.repeat_count
        QKeyEvent qKeyEvent(keyType, keyCode, modifiers,
                            event.scan_code, event.key_code, event.modifiers,
                            text);
        qKeyEvent.setTimestamp(timestamp);
        if (context->filterEvent(&qKeyEvent)) {
            // key event filtered out by input context
            return;
        }
    }

    mQtWindowSystem->handleExtendedKeyEvent(timestamp, keyType, keyCode, modifiers,
            event.scan_code, event.key_code, event.modifiers, text);
}

void QtEventFeeder::dispatchMotion(MirMotionEvent const& event)
{
    if (!mQtWindowSystem->hasTargetWindow())
        return;

    const int mirMotionAction = event.action & MirEventActionMask;

    // Ignore the events that do not interest us (or that we currently don't support or know
    // how to translate into Qt events)
    if (mirMotionAction != mir_motion_action_move
            && mirMotionAction != mir_motion_action_down
            && mirMotionAction != mir_motion_action_up
            && mirMotionAction != mir_motion_action_pointer_down
            && mirMotionAction != mir_motion_action_pointer_up
            && mirMotionAction != mir_motion_action_cancel) {
        return;
    }


    // FIXME(loicm) Max pressure is device specific. That one is for the Samsung Galaxy Nexus. That
    //     needs to be fixed as soon as the compat input lib adds query support.
    const float kMaxPressure = 1.28;
    const QRect kWindowGeometry = mQtWindowSystem->targetWindowGeometry();
    QList<QWindowSystemInterface::TouchPoint> touchPoints;

    // TODO: Is it worth setting the Qt::TouchPointStationary ones? Currently they are left
    //       as Qt::TouchPointMoved
    const int kPointerCount = (int) event.pointer_count;
    for (int i = 0; i < kPointerCount; ++i) {
        QWindowSystemInterface::TouchPoint touchPoint;

        const float kX = event.pointer_coordinates[i].x;
        const float kY = event.pointer_coordinates[i].y;
        const float kW = event.pointer_coordinates[i].touch_major;
        const float kH = event.pointer_coordinates[i].touch_minor;
        const float kP = event.pointer_coordinates[i].pressure;
        touchPoint.id = event.pointer_coordinates[i].id;
        touchPoint.normalPosition = QPointF(kX / kWindowGeometry.width(), kY / kWindowGeometry.height());
        touchPoint.area = QRectF(kX - (kW / 2.0), kY - (kH / 2.0), kW, kH);
        touchPoint.pressure = kP / kMaxPressure;
        touchPoint.state = Qt::TouchPointMoved;

        touchPoints.append(touchPoint);
    }

    switch (mirMotionAction) {
    case mir_motion_action_move:
        // No extra work needed.
        break;

    case mir_motion_action_down:
        // NB: hardcoded index 0 because there's only a single touch point in this case
        touchPoints[0].state = Qt::TouchPointPressed;
        break;

    case mir_motion_action_up:
        touchPoints[0].state = Qt::TouchPointReleased;
        break;

    case mir_motion_action_pointer_down: {
        const int index = (event.action & MirEventActionPointerIndexMask) >>
            MirEventActionPointerIndexShift;
        touchPoints[index].state = Qt::TouchPointPressed;
        break;
        }

    case mir_motion_action_cancel:
    case mir_motion_action_pointer_up: {
        const int index = (event.action & MirEventActionPointerIndexMask) >>
            MirEventActionPointerIndexShift;
        touchPoints[index].state = Qt::TouchPointReleased;
        break;
        }

    case mir_motion_action_outside:
    case mir_motion_action_hover_move:
    case mir_motion_action_scroll:
    case mir_motion_action_hover_enter:
    case mir_motion_action_hover_exit:
    default:
        // Should never reach this point. If so, it's a programming error.
        qFatal("Trying to handle unsupported motion event action");
    }

    // Qt needs a happy, sane stream of touch events. So let's make sure we're not forwarding
    // any insanity.
    validateTouches(touchPoints);

    // Touch event propagation.
    mQtWindowSystem->handleTouchEvent(
            event.event_time / 1000000, //scales down the nsec_t (int64) to fit a ulong, precision lost but time difference suitable
            mTouchDevice,
            touchPoints);
}

void QtEventFeeder::start()
{
    // not used
}

void QtEventFeeder::stop()
{
    // not used
}

void QtEventFeeder::configuration_changed(nsecs_t when)
{
    Q_UNUSED(when);
}

void QtEventFeeder::device_reset(int32_t device_id, nsecs_t when)
{
    Q_UNUSED(device_id);
    Q_UNUSED(when);
}

void QtEventFeeder::validateTouches(QList<QWindowSystemInterface::TouchPoint> &touchPoints)
{
    QSet<int> updatedTouches;

    {
        int i = 0;
        while (i < touchPoints.count()) {
            bool mustDiscardTouch = !validateTouch(touchPoints[i]);
            if (mustDiscardTouch) {
                touchPoints.removeAt(i);
            } else {
                updatedTouches.insert(touchPoints.at(i).id);
                ++i;
            }
        }
    }

    // Release all unmentioned touches.
    {
        QHash<int, QWindowSystemInterface::TouchPoint>::iterator it = mActiveTouches.begin();
        while (it != mActiveTouches.end()) {
            if (!updatedTouches.contains(it.key())) {
                qCWarning(QTMIR_MIR_INPUT)
                    << "There's a touch (id =" << it.key() << ") missing. Releasing it.";
                it.value().state = Qt::TouchPointReleased;
                touchPoints.append(it.value());
                it = mActiveTouches.erase(it);
            } else {
                ++it;
            }
        }
    }
}

bool QtEventFeeder::validateTouch(QWindowSystemInterface::TouchPoint &touchPoint)
{
    bool ok = true;

    switch (touchPoint.state) {
    case Qt::TouchPointPressed:
        if (mActiveTouches.contains(touchPoint.id)) {
            qCWarning(QTMIR_MIR_INPUT)
                << "Would press an already existing touch (id =" << touchPoint.id
                << "). Making it move instead.";
            touchPoint.state = Qt::TouchPointMoved;
        }
        mActiveTouches[touchPoint.id] = touchPoint;
        break;
    case Qt::TouchPointMoved:
        if (!mActiveTouches.contains(touchPoint.id)) {
            qCWarning(QTMIR_MIR_INPUT)
                << "Would move a touch that wasn't pressed before (id =" << touchPoint.id
                << "). Making it press instead.";
            touchPoint.state = Qt::TouchPointPressed;
        }
        mActiveTouches[touchPoint.id] = touchPoint;
        break;
    case Qt::TouchPointStationary:
        if (!mActiveTouches.contains(touchPoint.id)) {
            qCWarning(QTMIR_MIR_INPUT)
                << "There's an stationary touch that wasn't pressed before (id =" << touchPoint.id
                << "). Making it press instead.";
            touchPoint.state = Qt::TouchPointPressed;
        }
        mActiveTouches[touchPoint.id] = touchPoint;
        break;
    case Qt::TouchPointReleased:
        if (!mActiveTouches.contains(touchPoint.id)) {
            qCWarning(QTMIR_MIR_INPUT)
                << "Would release a touch that wasn't pressed before (id =" << touchPoint.id
                << "). Ignoring it.";
            ok = false;
        } else {
            mActiveTouches.remove(touchPoint.id);
        }
        break;
    default:
        qFatal("QtEventFeeder: invalid touch state");
    }

    return ok;
}
