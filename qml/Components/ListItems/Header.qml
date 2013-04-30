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
    /*!
     \preliminary
     The text that is shown as the header text.
     \qmlproperty string text
    */
    property alias text: label.text

    /* When this header is used in a ListViewWithPageHeader with CurrentLabelAtStart mode, to stop
       the list's delegates contents being visible through this header, they clip themselves
       depending on their position. However to allow delegates underlap the bottom X pixels of this
       header (so delegate appears through ThinDivider say) we set an offset property here. */
    readonly property int bottomBorderAllowedOverlap: bottomBorder.allowedBottomOverlap

    height: units.gu(4.5)

    Item {
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            bottom: bottomBorder.top
        }

        Label {
            id: label
            anchors {
                left: parent.left
                leftMargin: units.gu(2)
                right: parent.right
                rightMargin: units.gu(0.5)
                verticalCenter: parent.verticalCenter
            }

            color: "#f3f3e7" //at 60%
            opacity: 0.6
            font.family: "Ubuntu"
            fontSize: "medium"
            elide: Text.ElideRight
            textFormat: Text.PlainText
            style: Text.Raised
            styleColor: "black"
        }
    }

    ThinDivider {
        id: bottomBorder
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
        }
    }
}
