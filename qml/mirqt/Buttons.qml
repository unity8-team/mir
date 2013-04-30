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
    objectName: "Buttons"
    title: i18n.tr("Buttons")

    Column {
        spacing: units.gu(4)

        TemplateRow {
            title: i18n.tr("Standard")

            Button {
                text: i18n.tr("Call")
            }
        }

        TemplateRow {
            title: i18n.tr("Disabled")

            Button {
                text: i18n.tr("Call")
                enabled: false
            }
        }

        TemplateRow {
            title: i18n.tr("Colors")

            Button {
                text: i18n.tr("Call")
                color: "#5da357"
            }

            Button {
                text: i18n.tr("Call")
            }

            Rectangle {
                id: darkBackground
                width: darkButton.width + units.gu(3)
                height: darkButton.height + units.gu(3)
                color: "#3a3c41"

                Button {
                    id: darkButton
                    text: i18n.tr("Call")
                    ItemStyle.class: "dark-button"
                    anchors.centerIn: parent
                }
            }
        }

        TemplateRow {
            title: i18n.tr("Content")

            Button {
                text: i18n.tr("Call")
            }

            Button {
                iconSource: "call_icon.png"
            }

            Button {
                width: units.gu(11)
                text: i18n.tr("Call")
                iconSource: "call_icon.png"
            }

            Button {
                width: units.gu(11)
                text: i18n.tr("Call")
                iconSource: "call_icon.png"
                iconPosition: "right"
            }
        }

        TemplateRow {
            title: i18n.tr("Scalability")

            Button {
                text: i18n.tr("Call")
            }

            Button {
                text: i18n.tr("Call")
                width: units.gu(18)
                height: units.gu(11)
            }
        }
    }
}
