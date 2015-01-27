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
 */

#include "debughelpers.h"
#include <QTouchEvent>

#include <mir_toolkit/event.h>

// Unity API
#include <unity/shell/application/ApplicationInfoInterface.h>

const char *touchPointStateToString(Qt::TouchPointState state)
{
    switch (state) {
    case Qt::TouchPointPressed:
        return "pressed";
    case Qt::TouchPointMoved:
        return "moved";
    case Qt::TouchPointStationary:
        return "stationary";
    case Qt::TouchPointReleased:
        return "released";
    default:
        return "UNKNOWN!";
    }
}

QString touchEventToString(const QTouchEvent *ev)
{
    QString message;

    switch (ev->type()) {
    case QEvent::TouchBegin:
        message.append("TouchBegin ");
        break;
    case QEvent::TouchUpdate:
        message.append("TouchUpdate ");
        break;
    case QEvent::TouchEnd:
        message.append("TouchEnd ");
        break;
    case QEvent::TouchCancel:
        message.append("TouchCancel ");
    default:
        message.append("TouchUNKNOWN ");
    }

    for (int i=0; i < ev->touchPoints().size(); ++i) {

        const QTouchEvent::TouchPoint& touchPoint = ev->touchPoints().at(i);
        message.append(
            QString("(id:%1, state:%2, scenePos:(%3,%4), pos:(%5,%6)) ")
                .arg(touchPoint.id())
                .arg(touchPointStateToString(touchPoint.state()))
                .arg(touchPoint.scenePos().x())
                .arg(touchPoint.scenePos().y())
                .arg(touchPoint.pos().x())
                .arg(touchPoint.pos().y())
            );
    }

    return message;
}

QString mirSurfaceAttribAndValueToString(MirSurfaceAttrib attrib, int value)
{
    QString str;

    switch (attrib) {
    case mir_surface_attrib_type:
        str = QString("type=%1").arg(mirSurfaceTypeToStr(value));
        break;

    case mir_surface_attrib_state:
        str = QString("state=%1").arg(mirSurfaceStateToStr(value));
        break;

    case mir_surface_attrib_swapinterval:
        str = QString("swapinterval=%1").arg(value);
        break;

    case mir_surface_attrib_focus:
        str = QString("focus=%1").arg(mirSurfaceFocusStateToStr(value));
        break;

    case mir_surface_attrib_dpi:
        str = QString("dpi=%1").arg(value);
        break;

    case mir_surface_attrib_visibility:
        str = QString("visibility=%1").arg(mirSurfaceVisibilityToStr(value));
        break;
    default:
        str = QString("type'%1'=%2").arg((int)attrib).arg(value);
    }

    return str;
}

const char *mirSurfaceTypeToStr(int value)
{
    switch (value) {
    case mir_surface_type_normal:
        return "normal";
    case mir_surface_type_utility:
        return "utility";
    case mir_surface_type_dialog:
        return "dialog";
    case mir_surface_type_overlay:
        return "overlay";
    case mir_surface_type_freestyle:
        return "freestyle";
    case mir_surface_type_popover:
        return "popover";
    case mir_surface_type_inputmethod:
        return "inputmethod";
    default:
        return "???";
    }
}

const char *mirSurfaceStateToStr(int value)
{
    switch (value) {
    case mir_surface_state_unknown:
        return "unknown";
    case mir_surface_state_restored:
        return "restored";
    case mir_surface_state_minimized:
        return "minimized";
    case mir_surface_state_maximized:
        return "maximized";
    case mir_surface_state_vertmaximized:
        return "vertmaximized";
    case mir_surface_state_fullscreen:
        return "fullscreen";
    default:
        return "???";
    }
}

const char *mirSurfaceFocusStateToStr(int value)
{
    switch (value) {
    case mir_surface_unfocused:
        return "unfocused";
    case mir_surface_focused:
        return "focused";
    default:
        return "???";
    }
}

const char *mirSurfaceVisibilityToStr(int value)
{
    switch (value) {
    case mir_surface_visibility_occluded:
        return "occluded";
    case mir_surface_visibility_exposed:
        return "exposed";
    default:
        return "???";
    }
}

const char *mirTouchActionToStr(MirTouchInputEventTouchAction action)
{
    switch (action) {
    case mir_touch_input_event_action_up:
        return "up";
    case mir_touch_input_event_action_down:
        return "down";
    case mir_touch_input_event_action_change:
        return "change";
    default:
        return "???";
    }
}

using namespace unity::shell::application;

const char *applicationStateToStr(int state)
{
    switch (state) {
    case ApplicationInfoInterface::Starting:
        return "starting";
    case ApplicationInfoInterface::Running:
        return "running";
    case ApplicationInfoInterface::Suspended:
        return "suspended";
    case ApplicationInfoInterface::Stopped:
        return "stopped";
    default:
        return "???";
    }
}
