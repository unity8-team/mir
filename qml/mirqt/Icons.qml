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

Template {
    title: i18n.tr("Icons")
    objectName: "Icons"

    Column {
        spacing: units.gu(4)

        TemplateRow {
            title: i18n.tr("scaling")
            spacing: units.gu(2)

            Icon {
                name: "call-start"
                width: 16
                height: 16
            }

            Icon {
                name: "call-start"
                width: 48
                height: 48
            }
        }

        TemplateRow {
            title: i18n.tr("colorization")
            spacing: units.gu(2)

            Icon {
                name: "computer-symbolic"
                width: 24
                height: 24
            }

            Icon {
                name: "computer-symbolic"
                color: "red"
                keyColor: "#bebebe"
                width: 24
                height: 24
            }
        }
    }
}
