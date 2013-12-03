#include "qteventfeeder.h"

#include <qpa/qwindowsysteminterface.h>
#include <QGuiApplication>

#include <QDebug>

using namespace android;

QtEventFeeder::QtEventFeeder()
{
  // Initialize touch device. Hardcoded just like in qtubuntu
  mTouchDevice = new QTouchDevice();
  mTouchDevice->setType(QTouchDevice::TouchScreen);
  mTouchDevice->setCapabilities(
      QTouchDevice::Position | QTouchDevice::Area | QTouchDevice::Pressure |
      QTouchDevice::NormalizedPosition);
  QWindowSystemInterface::registerTouchDevice(mTouchDevice);
}

void QtEventFeeder::notifyConfigurationChanged(const NotifyConfigurationChangedArgs* args)
{
    (void)args;
}

void QtEventFeeder::notifyKey(const NotifyKeyArgs* args)
{
    (void)args;
}

void QtEventFeeder::notifyMotion(const NotifyMotionArgs* args)
{
    if (QGuiApplication::topLevelWindows().isEmpty())
        return;

    QWindow *window = QGuiApplication::topLevelWindows().first();

    // FIXME(loicm) Max pressure is device specific. That one is for the Samsung Galaxy Nexus. That
    //     needs to be fixed as soon as the compat input lib adds query support.
    const float kMaxPressure = 1.28;
    const QRect kWindowGeometry = window->geometry();
    QList<QWindowSystemInterface::TouchPoint> touchPoints;

    // TODO: Is it worth setting the Qt::TouchPointStationary ones? Currently they are left
    //       as Qt::TouchPointMoved
    const int kPointerCount = (int) args->pointerCount;
    for (int i = 0; i < kPointerCount; ++i) {
        QWindowSystemInterface::TouchPoint touchPoint;

        const float kX = args->pointerCoords[i].getX();
        const float kY = args->pointerCoords[i].getY();
        const float kW = args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR);
        const float kH = args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR);
        const float kP = args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_PRESSURE);
        touchPoint.id = args->pointerProperties[i].id;
        touchPoint.normalPosition = QPointF(kX / kWindowGeometry.width(), kY / kWindowGeometry.height());
        touchPoint.area = QRectF(kX - (kW / 2.0), kY - (kH / 2.0), kW, kH);
        touchPoint.pressure = kP / kMaxPressure;
        touchPoint.state = Qt::TouchPointMoved;

        touchPoints.append(touchPoint);
    }

    switch (args->action & AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_MOVE:
        // No extra work needed.
        break;

    case AMOTION_EVENT_ACTION_DOWN:
        // NB: hardcoded index 0 because there's only a single touch point in this case
        touchPoints[0].state = Qt::TouchPointPressed;
        break;

    case AMOTION_EVENT_ACTION_UP:
        touchPoints[0].state = Qt::TouchPointReleased;
        break;

    case AMOTION_EVENT_ACTION_POINTER_DOWN: {
        const int index = (args->action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
            AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        touchPoints[index].state = Qt::TouchPointPressed;
        break;
        }

    case AMOTION_EVENT_ACTION_CANCEL:
    case AMOTION_EVENT_ACTION_POINTER_UP: {
        const int index = (args->action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
            AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        touchPoints[index].state = Qt::TouchPointReleased;
        break;
        }

    case AMOTION_EVENT_ACTION_OUTSIDE:
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
    case AMOTION_EVENT_ACTION_SCROLL:
    case AMOTION_EVENT_ACTION_HOVER_ENTER:
    case AMOTION_EVENT_ACTION_HOVER_EXIT:
        default:
        qWarning() << "unhandled motion event action" << (int)(args->action & AMOTION_EVENT_ACTION_MASK);
    }

    // Touch event propagation.
    QWindowSystemInterface::handleTouchEvent(
            window,
            args->eventTime / 1000000,
            mTouchDevice,
            touchPoints);
}

void QtEventFeeder::notifySwitch(const NotifySwitchArgs* args)
{
    (void)args;
}

void QtEventFeeder::notifyDeviceReset(const NotifyDeviceResetArgs* args)
{
    (void)args;
}
