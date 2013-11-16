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

import QtQuick 2.0
import QtQuick.Window 2.0
import Unity.Application 0.1

Item {
    id: root
    property variant __oskSurface: null
    readonly property bool enabled: __oskSurface !== null

    UbuntuKeyboardInfo {
        id: ubuntuKeyboardInfo
    }

    Connections {
        target: SurfaceManager
        onSurfaceCreated: {
            // FIXME(greyback) Not using "type" as it set after surface created
            if (surface.name === "MaliitOnScreenKeyboard") {
                __oskSurface = surface;
            }
        }
        onSurfaceDestroyed: {
            if (surface.name === "MaliitOnScreenKeyboard") {
                __oskSurface = null;
            }
        }
    }

    /* Do not let touches that are meant for the OSK hit the Shell components behind it. */
    MouseArea {
        enabled: oskInputArea.enabled
        anchors.fill: oskInputArea
    }

    /* Need to position InputArea of oskSurface to cover the keyboard. */
    InputArea {
        id: oskInputArea

        x: ubuntuKeyboardInfo.x
        y: ubuntuKeyboardInfo.y
        width: ubuntuKeyboardInfo.width
        height: ubuntuKeyboardInfo.height

        surface: __oskSurface
        enabled: false
    }

    Connections {
        target: __oskSurface

        onStateChanged: {
            if (__oskSurface.state === MirSurface.Maximized) {
                __oskSurface.visible = true;
                oskInputArea.enabled = true;
            } else {
                __oskSurface.visible = false;
                oskInputArea.enabled = false;
            }
        }
    }
}
