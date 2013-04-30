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

import QtQuick 2.0

Column {
    id: shortcutsContainer

    property int foldedAngle: 0
    property int initialFoldedOffset: 0
    property int foldedOffset: 0
    property int initialIndex: 0

    property bool animating: angleAnimation.running || offsetAnimation.running

    property ListModel model
    property var delegate

    Behavior on foldedAngle { NumberAnimation { id: angleAnimation; duration: 200; } }
    Behavior on foldedOffset { NumberAnimation { id: offsetAnimation; duration: 200; } }

    Repeater {
        model: shortcutsContainer.model
        delegate: shortcutsContainer.delegate
    }
}
