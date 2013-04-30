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

Rectangle {
    id: highlight

    property bool pressed: false
    property real pressedOpacity: 0.3

    color: "black"
    opacity: 0
    state: pressed ? "pressed" : ""
    states: [
        State {
            name: "pressed"
            PropertyChanges { target: highlight; opacity: pressedOpacity}
        }
    ]
    transitions: [
        Transition {
            from: ""
            to: "pressed"
            NumberAnimation { target: highlight; property: "opacity"; duration: 50}
        },
        Transition {
            from: "pressed"
            to: ""
            NumberAnimation { target: highlight; property: "opacity"; duration: 100}
        }
    ]
}
