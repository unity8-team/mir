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

// Local
#include "ubuntuinput.h"
#include "ubuntuclientintegration.h"
#include "ubuntunativeinterface.h"
#include <logging.h>

// Qt
#if !defined(QT_NO_DEBUG)
#include <QtCore/QThread>
#endif
#include <QtCore/qglobal.h>
#include <QtCore/QCoreApplication>
#include <private/qguiapplication_p.h>
#include <qpa/qplatforminputcontext.h>
#include <qpa/qwindowsysteminterface.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

// Platform API
#include <ubuntu/application/ui/input/event.h>

#define LOG_EVENTS 0

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

// Lookup table for the key types.
// FIXME(loicm) Not sure what to do with that multiple thing.
static const QEvent::Type kEventType[] = {
  QEvent::KeyPress,    // U_KEY_ACTION_DOWN     = 0
  QEvent::KeyRelease,  // U_KEY_ACTION_UP       = 1
  QEvent::KeyPress     // U_KEY_ACTION_MULTIPLE = 2
};


class UbuntuEvent : public QEvent
{
public:
    UbuntuEvent(QWindow* window, const Event* event, QEvent::Type type)
        : QEvent(type), window(window) {
        memcpy(&nativeEvent, event, sizeof(Event));
    }
    QWindow* window;
    Event nativeEvent;
};

UbuntuInput::UbuntuInput(UbuntuClientIntegration* integration)
    : QObject(nullptr)
    , mIntegration(integration)
    , mEventFilterType(static_cast<UbuntuNativeInterface*>(
        integration->nativeInterface())->genericEventFilterType())
    , mEventType(static_cast<QEvent::Type>(QEvent::registerEventType()))
{
    // Initialize touch device.
    mTouchDevice = new QTouchDevice;
    mTouchDevice->setType(QTouchDevice::TouchScreen);
    mTouchDevice->setCapabilities(
            QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure |
            QTouchDevice::NormalizedPosition);
    QWindowSystemInterface::registerTouchDevice(mTouchDevice);
}

UbuntuInput::~UbuntuInput()
{
  // Qt will take care of deleting mTouchDevice.
}

void UbuntuInput::customEvent(QEvent* event)
{
    DASSERT(QThread::currentThread() == thread());
    UbuntuEvent* ubuntuEvent = static_cast<UbuntuEvent*>(event);

    // Event filtering.
    long result;
    if (QWindowSystemInterface::handleNativeEvent(
                ubuntuEvent->window, mEventFilterType, &ubuntuEvent->nativeEvent, &result) == true) {
        DLOG("event filtered out by native interface");
        return;
    }

    // Event dispatching.
    switch (ubuntuEvent->nativeEvent.type) {
    case MOTION_EVENT_TYPE:
        dispatchMotionEvent(ubuntuEvent->window, &ubuntuEvent->nativeEvent);
        break;
    case KEY_EVENT_TYPE:
        dispatchKeyEvent(ubuntuEvent->window, &ubuntuEvent->nativeEvent);
        break;
    case HW_SWITCH_EVENT_TYPE:
        dispatchHWSwitchEvent(ubuntuEvent->window, &ubuntuEvent->nativeEvent);
        break;
    default:
        DLOG("unhandled event type %d", ubuntuEvent->nativeEvent.type);
    }
}

void UbuntuInput::postEvent(QWindow* window, const void* event)
{
    QCoreApplication::postEvent(this, new UbuntuEvent(
            window, reinterpret_cast<const Event*>(event), mEventType));

    if ((window->flags() && Qt::WindowTransparentForInput) && window->parent()) {
        QCoreApplication::postEvent(this, new UbuntuEvent(
                    window->parent(), reinterpret_cast<const Event*>(event), mEventType));
    }
}

void UbuntuInput::dispatchMotionEvent(QWindow* window, const void* ev)
{
    const Event* event = reinterpret_cast<const Event*>(ev);

    #if (LOG_EVENTS != 0)
    // Motion event logging.
    LOG("MOTION device_id:%d source_id:%d action:%d flags:%d meta_state:%d edge_flags:%d "
            "button_state:%d x_offset:%.2f y_offset:%.2f x_precision:%.2f y_precision:%.2f "
            "down_time:%lld event_time:%lld pointer_count:%d {", event->device_id,
            event->source_id, event->action, event->flags, event->meta_state,
            event->details.motion.edge_flags, event->details.motion.button_state,
            event->details.motion.x_offset, event->details.motion.y_offset,
            event->details.motion.x_precision, event->details.motion.y_precision,
            event->details.motion.down_time, event->details.motion.event_time,
            event->details.motion.pointer_count);
    for (size_t i = 0; i < event->details.motion.pointer_count; i++) {
        LOG("  id:%d x:%.2f y:%.2f rx:%.2f ry:%.2f maj:%.2f min:%.2f sz:%.2f press:%.2f",
                event->details.motion.pointer_coordinates[i].id,
                event->details.motion.pointer_coordinates[i].x,
                event->details.motion.pointer_coordinates[i].y,
                event->details.motion.pointer_coordinates[i].raw_x,
                event->details.motion.pointer_coordinates[i].raw_y,
                event->details.motion.pointer_coordinates[i].touch_major,
                event->details.motion.pointer_coordinates[i].touch_minor,
                event->details.motion.pointer_coordinates[i].size,
                event->details.motion.pointer_coordinates[i].pressure
                // event->details.motion.pointer_coordinates[i].orientation  -> Always 0.0.
           );
    }
    LOG("}");
    #endif

    // FIXME(loicm) Max pressure is device specific. That one is for the Samsung Galaxy Nexus. That
    //     needs to be fixed as soon as the compat input lib adds query support.
    const float kMaxPressure = 1.28;
    const QRect kWindowGeometry = window->geometry();
    QList<QWindowSystemInterface::TouchPoint> touchPoints;


    // TODO: Is it worth setting the Qt::TouchPointStationary ones? Currently they are left
    //       as Qt::TouchPointMoved
    const int kPointerCount = event->details.motion.pointer_count;
    for (int i = 0; i < kPointerCount; ++i) {
        QWindowSystemInterface::TouchPoint touchPoint;

        const float kX = event->details.motion.pointer_coordinates[i].raw_x;
        const float kY = event->details.motion.pointer_coordinates[i].raw_y;
        const float kW = event->details.motion.pointer_coordinates[i].touch_major;
        const float kH = event->details.motion.pointer_coordinates[i].touch_minor;
        const float kP = event->details.motion.pointer_coordinates[i].pressure;
        touchPoint.id = event->details.motion.pointer_coordinates[i].id;
        touchPoint.normalPosition = QPointF(kX / kWindowGeometry.width(), kY / kWindowGeometry.height());
        touchPoint.area = QRectF(kX - (kW / 2.0), kY - (kH / 2.0), kW, kH);
        touchPoint.pressure = kP / kMaxPressure;
        touchPoint.state = Qt::TouchPointMoved;

        touchPoints.append(touchPoint);
    }

    switch (event->action & U_MOTION_ACTION_MASK) {
    case U_MOTION_ACTION_MOVE:
        // No extra work needed.
        break;

    case U_MOTION_ACTION_DOWN:
        touchPoints[0].state = Qt::TouchPointPressed;
        break;

    case U_MOTION_ACTION_UP:
        touchPoints[0].state = Qt::TouchPointReleased;
        break;

    case U_MOTION_ACTION_POINTER_DOWN: {
        const int index = (event->action & U_MOTION_ACTION_POINTER_INDEX_MASK) >>
            U_MOTION_ACTION_POINTER_INDEX_SHIFT;
        touchPoints[index].state = Qt::TouchPointPressed;
        break;
    }

    case U_MOTION_ACTION_CANCEL:
    case U_MOTION_ACTION_POINTER_UP: {
        const int index = (event->action & U_MOTION_ACTION_POINTER_INDEX_MASK) >>
            U_MOTION_ACTION_POINTER_INDEX_SHIFT;
        touchPoints[index].state = Qt::TouchPointReleased;
        break;
    }

    case U_MOTION_ACTION_OUTSIDE:
    case U_MOTION_ACTION_HOVER_MOVE:
    case U_MOTION_ACTION_SCROLL:
    case U_MOTION_ACTION_HOVER_ENTER:
    case U_MOTION_ACTION_HOVER_EXIT:
    default:
        DLOG("unhandled motion event action %d", event->action & U_MOTION_ACTION_MASK);
    }

    QWindowSystemInterface::handleTouchEvent(window, event->details.motion.event_time / 1000000,
            mTouchDevice, touchPoints);
}

static uint32_t translateKeysym(uint32_t sym, char *string, size_t size)
{
    Q_UNUSED(size);
    string[0] = '\0';

    if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F35)
        return Qt::Key_F1 + (int(sym) - XKB_KEY_F1);

    for (int i = 0; KeyTable[i]; i += 2) {
        if (sym == KeyTable[i])
            return KeyTable[i + 1];
    }

    string[0] = sym;
    string[1] = '\0';
    return toupper(sym);
}

void UbuntuInput::dispatchKeyEvent(QWindow* window, const void* ev)
{
    const Event* event = reinterpret_cast<const Event*>(ev);

    #if (LOG_EVENTS != 0)
    // Key event logging.
    LOG("KEY device_id:%d source_id:%d action:%d flags:%d meta_state:%d key_code:%d "
            "scan_code:%d repeat_count:%d down_time:%lld event_time:%lld is_system_key:%d",
            event->device_id, event->source_id, event->action, event->flags, event->meta_state,
            event->details.key.key_code, event->details.key.scan_code,
            event->details.key.repeat_count, event->details.key.down_time,
            event->details.key.event_time, event->details.key.is_system_key);
    #endif

    ulong timestamp = event->details.key.event_time / 1000000;
    xkb_keysym_t xk_sym = (xkb_keysym_t)event->details.key.key_code;

    // Key modifier and unicode index mapping.
    const int kMetaState = event->meta_state;
    Qt::KeyboardModifiers modifiers = Qt::NoModifier;
    if (kMetaState & U_KEY_MODIFIER_SHIFT) {
        modifiers |= Qt::ShiftModifier;
    }
    if (kMetaState & U_KEY_MODIFIER_CTRL) {
        modifiers |= Qt::ControlModifier;
    }
    if (kMetaState & U_KEY_MODIFIER_ALT) {
        modifiers |= Qt::AltModifier;
    }
    if (kMetaState & U_KEY_MODIFIER_META) {
        modifiers |= Qt::MetaModifier;
    }

    QEvent::Type keyType = event->action == U_KEY_ACTION_DOWN ? QEvent::KeyPress : QEvent::KeyRelease;

    char s[2];
    int sym = translateKeysym(xk_sym, s, sizeof(s));
    QString text = QString::fromLatin1(s);

    QPlatformInputContext* context = QGuiApplicationPrivate::platformIntegration()->inputContext();
    if (context) {
        QKeyEvent qKeyEvent(keyType, sym, modifiers, text);
        qKeyEvent.setTimestamp(timestamp);
        if (context->filterEvent(&qKeyEvent)) {
            DLOG("key event filtered out by input context");
            return;
        }
    }

    QWindowSystemInterface::handleKeyEvent(window, timestamp, keyType, sym, modifiers, text);
}

void UbuntuInput::dispatchHWSwitchEvent(QWindow* window, const void* ev)
{
    Q_UNUSED(window);
    Q_UNUSED(ev);

    #if (LOG_EVENTS != 0)
    // HW switch event logging.
    const Event* event = reinterpret_cast<const Event*>(ev);
    LOG("HWSWITCH device_id:%d source_id:%d action:%d flags:%d meta_state:%d event_time:%lld "
            "policy_flags:%u switch_values:%d switch_mask:%d", event->device_id, event->source_id,
            event->action, event->flags, event->meta_state, event->details.hw_switch.event_time,
            event->details.hw_switch.policy_flags, event->details.hw_switch.switch_values,
            event->details.hw_switch.switch_mask);
    #endif

    // FIXME(loicm) Not sure how to interpret that kind of event.
    DLOG("hw switch events are not handled");
}
