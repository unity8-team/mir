/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "debughelpers.h"
#include <QTouchEvent>

QString touchPointStateToString(Qt::TouchPointState state)
{
    switch (state) {
    case Qt::TouchPointPressed:
        return QString("pressed");
    case Qt::TouchPointMoved:
        return QString("moved");
    case Qt::TouchPointStationary:
        return QString("stationary");
    default: // Qt::TouchPointReleased:
        return QString("released");
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
    default: //QEvent::TouchCancel
        message.append("TouchCancel ");
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
