/*
 * Copyright © 2013-2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Daniel d'Andrada <daniel.dandrada@canonical.com>
 */

#ifndef MIR_QT_EVENT_FEEDER_H
#define MIR_QT_EVENT_FEEDER_H

#include <mir/input/input_dispatcher.h>
#include <mir/shell/input_targeter.h>

class QTouchDevice;

/*
  Fills Qt's event loop with input events from Mir
 */
class QtEventFeeder : public mir::input::InputDispatcher
{
public:
    QtEventFeeder();

    static const int MirEventActionMask;
    static const int MirEventActionPointerIndexMask;
    static const int MirEventActionPointerIndexShift;

    void configuration_changed(nsecs_t when) override;
    void device_reset(int32_t device_id, nsecs_t when) override;
    void dispatch(MirEvent const& event) override;
    void start() override;
    void stop() override;

private:
    void dispatchKey(MirKeyEvent const& event);
    void dispatchMotion(MirMotionEvent const& event);

    QTouchDevice *mTouchDevice;
};

#endif // MIR_QT_EVENT_FEEDER_H
