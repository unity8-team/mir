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
