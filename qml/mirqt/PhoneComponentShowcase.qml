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
import Ubuntu.Components.Popups 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem

Rectangle {
    objectName: "phoneComponentShowcase"
    color: "lightgrey"
    width: units.gu(45)
    height: units.gu(80)

    Tabs {
        objectName: "Tabs"
        ItemStyle.class: "new-tabs"
        anchors.fill: parent

        Tab {
            objectName: "Components"
            title: "Components"
            page: Flickable {
                objectName: "Flickable"
                clip: true
                width: units.gu(45)
                height: units.gu(80)

                contentWidth: width
                contentHeight: background.height

                Rectangle {
                    id: background
                    width: parent.width
                    height: childrenRect.height
                    color: "#e6e6e6"

                    Column {
                        width: parent.width
                        height: childrenRect.height

                        ListItem.SingleControl {
                            control: Button {
                                anchors {
                                    margins: units.gu(1)
                                    fill: parent
                                }
                                text: i18n.tr("Button")
                            }
                        }
                        ListItem.Standard {
                            text: i18n.tr("Checkbox")
                            control: CheckBox { objectName: "Checkbox" }
                        }
                        ListItem.Standard {
                            text: i18n.tr("Switch")
                            control: Switch { objectName: "Switch" }
                        }
                        ListItem.Header { text: i18n.tr("Slider") }
                        ListItem.SingleControl {
                            control: Slider {
                                objectName: "Slider" 
                                anchors {
                                    margins: units.gu(1)
                                    fill: parent
                                }
                                value: 50
                            }
                        }
                        ListItem.Empty {
                            highlightWhenPressed: false
                            Label {
                                anchors.left: parent.left
                                text: i18n.tr("Activity indicator")
                                anchors {
                                    verticalCenter: parent.verticalCenter
                                    margins: units.gu(1)
                                }
                            }
                            ActivityIndicator {
                                objectName: "Activity Indicator"
                                running: true
                                anchors {
                                    margins: units.gu(1)
                                    right: parent.right
                                    verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                        ListItem.Header { text: i18n.tr("Progress bar") }
                        ListItem.Empty {
                            highlightWhenPressed: false
                            ProgressBar {
                                objectName: "ProgressBar"
                                id: progress
                                anchors.centerIn: parent
                                value: progress.minimumValue

                                SequentialAnimation on value {
                                    loops: Animation.Infinite
                                    NumberAnimation {
                                        from: progress.minimumValue
                                        to: progress.maximumValue
                                        duration: 2000
                                    }
                                    PauseAnimation {duration: 1000}
                                    ScriptAction { script: progress.indeterminate = true; }
                                    PauseAnimation {duration: 2000}
                                    ScriptAction {
                                        script: {
                                            progress.indeterminate = false
                                            progress.value = progress.minimumValue
                                        }
                                    }
                                }
                            }
                        }
                        ListItem.Header {
                            text: "Old-style tabs"
                        }
                        ListItem.Empty {
                            height: units.gu(40)
                            highlightWhenPressed: false
                            Tabs {
                                objectName: "Tabs (classic)"
                                anchors {
                                    fill: parent
                                    margins: units.gu(1)
                                }
                                Tab {
                                    objectName: "Tab 1"
                                    title: i18n.tr("Tab") + " 1"
                                    page: Rectangle {
                                        anchors.fill: parent
                                        color: "#eeeeee"
                                        Label {
                                            anchors.centerIn: parent
                                            text: i18n.tr("This is the first tab.")
                                            color: "#757373"
                                        }
                                    }
                                }
                                Tab {
                                    objectName: "Tab 2"
                                    iconSource: "call_icon.png"
                                    page: Rectangle {
                                        anchors.fill: parent
                                        color: "#e4e4e4"
                                        Label {
                                            anchors.centerIn: parent
                                            text: i18n.tr("This is the second tab.")
                                            color: "#757373"
                                        }
                                    }
                                }
                                Tab {
                                    objectName: "Tab 3"
                                    title: i18n.tr("Tab") + " 3"
                                    iconSource: "call_icon.png"
                                    page: Qt.resolvedUrl("MyCustomPage.qml")
                                }
                            }

                        }
                        ListItem.Empty {
                            highlightWhenPressed: false
                            TextField {
                                objectName: "TextField"
                                anchors.centerIn: parent
                                placeholderText: i18n.tr("simple text field")
                            }
                        }
                    }
                }
            }
        }

        Tab {
            objectName: "Ubuntu shape"
            title: "Ubuntu shape"
            page: UbuntuShapes {
                showHeader: false
            }
        }
    }
}
