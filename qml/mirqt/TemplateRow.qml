/*
 * Copyright 2012 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0
import Ubuntu.Components 0.1

Row {
    id: templateRow
    objectName: "TemplateRow"

    property string title

    spacing: units.gu(1)
    height: units.gu(6)

    Label {
        text: templateRow.title
        ItemStyle.class: "row-label"
        width: units.gu(10)
    }

    // ensure that all the children are vertically centered
    onChildrenChanged: {
        for (var i=0; i<children.length; i++) {
            children[i].anchors.verticalCenter = templateRow.verticalCenter;
        }
    }
}
