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

Item {
    id: root
    property var source: null
    readonly property bool ready: source && (image.status == Image.Ready)

    function scheduleUpdate() {
        image.source = "";
        if (source) {
            image.source = Qt.binding( function() {
                return source ? "image://screenshot/" + source.appId : "";
            });
        }
    }

    function updateFromCache() {
        scheduleUpdate();
    }

    Image {
        id: image
        anchors.fill: parent
        source: (root.source) ? "image://screenshot/" + root.source.appId : ""
        cache: false
    }
}
