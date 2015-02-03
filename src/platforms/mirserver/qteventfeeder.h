/*
 * Copyright (C) 2013-2015 Canonical, Ltd.
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
 */

#ifndef MIR_QT_EVENT_FEEDER_H
#define MIR_QT_EVENT_FEEDER_H

#include <mir/input/input_dispatcher.h>
#include <mir/shell/input_targeter.h>

#include <qpa/qwindowsysteminterface.h>

class QTouchDevice;

/*
  Fills Qt's event loop with input events from Mir
 */
class QtEventFeeder : public mir::input::InputDispatcher
{
public:
    // Interface between QtEventFeeder and the actual QWindowSystemInterface functions
    // and other related Qt methods and objects to enable replacing them with mocks in
    // pure unit tests.
    // TODO - Make it work with multimonitor scenarios
    class QtWindowSystemInterface {
        public:
        virtual ~QtWindowSystemInterface() {}
        virtual bool hasTargetWindow() = 0;
        virtual QRect targetWindowGeometry() = 0;
        virtual void registerTouchDevice(QTouchDevice *device) = 0;
        virtual void handleExtendedKeyEvent(ulong timestamp, QEvent::Type type, int key,
                Qt::KeyboardModifiers modifiers,
                quint32 nativeScanCode, quint32 nativeVirtualKey,
                quint32 nativeModifiers,
                const QString& text = QString(), bool autorep = false,
                ushort count = 1) = 0;
        virtual void handleTouchEvent(ulong timestamp, QTouchDevice *device,
                const QList<struct QWindowSystemInterface::TouchPoint> &points,
                Qt::KeyboardModifiers mods = Qt::NoModifier) = 0;
        virtual void handleMouseEvent(ulong timestamp, QPointF point, Qt::MouseButton buttons, Qt::KeyboardModifiers modifiers) = 0;
    };

    QtEventFeeder(QtWindowSystemInterface *windowSystem = nullptr);
    virtual ~QtEventFeeder();

    static const int MirEventActionMask;
    static const int MirEventActionPointerIndexMask;
    static const int MirEventActionPointerIndexShift;

    void configuration_changed(std::chrono::nanoseconds when) override;
    void device_reset(int32_t device_id, std::chrono::nanoseconds when) override;
    void dispatch(MirEvent const& event) override;
    void start() override;
    void stop() override;

private:
    void dispatchKey(MirInputEvent const* event);
    void dispatchTouch(MirInputEvent const* event);
    void dispatchPointer(MirInputEvent const* event);
    void validateTouches(QList<QWindowSystemInterface::TouchPoint> &touchPoints);
    bool validateTouch(QWindowSystemInterface::TouchPoint &touchPoint);

    QTouchDevice *mTouchDevice;
    QtWindowSystemInterface *mQtWindowSystem;

    // Maps the id of an active touch to its last known state
    QHash<int, QWindowSystemInterface::TouchPoint> mActiveTouches;
};

#endif // MIR_QT_EVENT_FEEDER_H
