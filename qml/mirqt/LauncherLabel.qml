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
import Ubuntu.Components 0.1

Item {
    id: launcherLabel

    property string iconSource: ""
    property string text: ""

    width: units.gu(13)
    height: units.gu(6)

    Image {
        id: bubbleImage
        width: parent.width
        height: parent.height
        y: -height/2
        source: "graphics/label_bubble.png"
    }

    Label {
        id: labelText
        anchors.right: labelImage.left
        anchors.rightMargin: units.gu(2)
        anchors.verticalCenter: labelImage.verticalCenter
        anchors.leftMargin: units.gu(1)
        text: launcherLabel.text
        color: "white"
        fontSize: "large"
        style: Text.Raised
        styleColor: "black"
    }

    UbuntuShape {
        id: labelImage

        anchors.right: bubbleImage.right
        anchors.top: bubbleImage.top
        anchors.margins: units.dp(5)
        width: units.gu(5)
        height: width * units.gu(7.5) / units.gu(8)
        borderSource: ""
        image: Image {
            source: launcherLabel.iconSource
            sourceSize { width: labelImage.width; height: labelImage.height }
        }
    }
}
