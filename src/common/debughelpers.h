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

#ifndef UBUNTUGESTURES_DEBUG_HELPER_H
#define UBUNTUGESTURES_DEBUG_HELPER_H

#include <QString>

#include <mir_toolkit/common.h>

class QTouchEvent;

const char *touchPointStateToString(Qt::TouchPointState state);
QString touchEventToString(const QTouchEvent *ev);

QString mirSurfaceAttribAndValueToString(MirSurfaceAttrib attrib, int value);
const char *mirSurfaceTypeToStr(int value);
const char *mirSurfaceStateToStr(int value);
const char *mirSurfaceFocusStateToStr(int value);
const char *mirSurfaceVisibilityToStr(int value);
const char *mirMotionActionToStr(int value);

const char *applicationStateToStr(int state);

#endif // UBUNTUGESTURES_DEBUG_HELPER_H
