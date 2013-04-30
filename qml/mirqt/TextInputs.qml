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
    objectName: "Text Input"
    title: i18n.tr("Text Input")
    id: page

    property string longText: "<i>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</i> "+
                              "<b>Nunc pretium iaculis risus, sed vehicula odio varius ac.</b> "+
                              "<u>Etiam orci lectus, bibendum in vulputate ac, tristique quis dui.</u> "+
                              "Suspendisse sed nulla in arcu dapibus vehicula ac eu tellus. "+
                              "Mauris malesuada nisl vitae neque auctor placerat. Ut vitae "+
                              "luctus libero. Praesent tempus nisl faucibus tellus semper "+
                              "eget iaculis purus congue. Donec elit tellus, malesuada non "+
                              "vehicula a, sagittis vel purus. Ut elementum vehicula accumsan. "+
                              "Pellentesque habitant morbi tristique senectus et netus et malesuada "+
                              "fames ac turpis egestas. Suspendisse ut rutrum sapien. Nullam id lobortis mauris"+
                              "Maecenas sollicitudin ipsum eu mauris vehicula rutrum. Proin aliquet "+
                              "justo ut mi luctus eu lobortis risus elementum. Cras sit amet "+
                              "sollicitudin magna. Quisque facilisis sodales tincidunt. Maecenas "+
                              "ligula tortor, luctus id elementum vel, varius vel augue. "+
                              "Nunc porta mattis bibendum. Nam vitae sapien ipsum, non viverra neque."

    Item {
        MouseArea {
            anchors.fill: parent
            onClicked: page.forceActiveFocus()
        }
        Flickable {
            anchors.fill: parent

            contentHeight: column.height
            clip: true
            id: flickable

            Column {
                spacing: units.gu(4)
                id: column

                Label {
                    text: "Single line"
                    ItemStyle.class: "title"
                }

                TemplateRow {
                    title: i18n.tr("Basic")

                    TextField {
                        id: textField1
                        KeyNavigation.tab: textFieldDisabled
                        KeyNavigation.backtab: expandFix

                        placeholderText: i18n.tr("simple text field")
                        hasClearButton: false
                    }
                }

                TemplateRow {
                    title: i18n.tr("Disabled")

                    TextField {
                        id: textFieldDisabled
                        KeyNavigation.tab: textField2
                        KeyNavigation.backtab: textField1

                        text: i18n.tr("disabled text field")
                        enabled: false
                    }
                }

                TemplateRow {
                    title: "Clear button"

                    TextField {
                        id: textField2
                        KeyNavigation.tab: textField3
                        KeyNavigation.backtab: textField1

                        placeholderText: i18n.tr("type to show clear button")
                        text: i18n.tr("sample text")
                    }
                }

                TemplateRow {
                    title: i18n.tr("Password")

                    TextField {
                        id: textField3
                        KeyNavigation.tab: richText
                        KeyNavigation.backtab: textField2

                        placeholderText: i18n.tr("password with echo")
                        echoMode: TextInput.Password

                    }
                }

                Label {
                    text: "Multi line"
                    ItemStyle.class: "title"
                }

                TemplateRow {
                    title: i18n.tr("Fixed")
                    height: richText.height

                    TextArea {
                        id: richText
                        textFormat:TextEdit.RichText
                        placeholderText: "the placeholder text is a text which is displayed when there is no content in the TextArea"
                        text: longText
                        KeyNavigation.tab: expandFix
                        KeyNavigation.backtab: textField3
                    }
                }

                TemplateRow {
                    title: i18n.tr("Expanding")
                    height: expandFix.height

                    TextArea {
                        id: expandFix
                        placeholderText: "expands till 5 lines, then scrolls"
                        autoSize: true
                        maximumLineCount: 5
                        KeyNavigation.tab: textField1
                        KeyNavigation.backtab: expandFix
                    }
                }
            }
        }
        Scrollbar {
            flickableItem: flickable
            align: Qt.AlignTrailing
        }
    }
}
