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
    id: shortcuts

    property ListModel model
    property ListModel shortcutsModel: model
    property ListModel openModel: ListModel{}
    property ListModel semiFoldedModel: ListModel{}
    property ListModel foldedModel: ListModel{}

    property bool enabled: true
    property bool releaseModeHighlight: false
    property string iconPath: "."

    property bool animating: openShortcuts.animating || semiFoldedShortcut.animating || foldedShortcuts.animating // read only
    property int currentIndex: -1

    property point releaseModeMouse: Qt.point(0, 0)
    property int scrollValue: 0
    property bool scrolling: scrollTimer.running

    property int itemWidth: units.gu(9)
    property int itemHeight: units.gu(7.5)
    property int iconWidth: units.gu(7)
    property int iconHeight: units.gu(6.5)

    property int iconCount: shortcutsModel.count

    property int foldedSize: itemWidth - foldedShortcuts.initialFoldedOffset

    property int openItemsCount
    property int foldedItemsCount: iconCount - openItemsCount
    property int fillItemHeight: height - openItemsCount * itemHeight - foldedItemsCount * foldedSize
    property bool open: false
    property bool inverted: true
    property int labelPosition
    labelPosition: {
        if (!inverted) {
            return currentIndex*shortcuts.itemHeight + shortcuts.itemHeight/2 + shortcutsContainer.targetY
        } else {
            return shortcuts.height - (currentIndex*shortcuts.itemHeight + shortcuts.itemHeight/2 + shortcutsContainer.targetY)
        }
    }

    rotation: inverted ? 180 : 0

    QtObject {
        id: priv

        property int modelCount: shortcuts.model ? shortcuts.model.count : 0
    }

    signal applicationSelected(string desktopFile)
    signal dashItemSelected(int index)

    Component.onCompleted: init()

    function init() {
        clearModels()
        addDashIcon()
        calculateOpenIcons()
        createModels()
    }

    function clearModels() {
        openModel.clear()
        semiFoldedModel.clear()
        foldedModel.clear()
    }

    function addDashIcon() {
        shortcutsModel.insert(0, {"desktopFile": "dash.desktop",
                                  "name": "Home",
                                  "icon": "dash",
                                  "exec": ""
                                  });
    }

    function calculateOpenIcons() {
        var foldedSize = (shortcuts.itemHeight - foldedShortcuts.initialFoldedOffset)
        var maxopenItemsCount = Math.floor(shortcuts.height / shortcuts.itemHeight)
        var leftOverIcons = shortcuts.iconCount - maxopenItemsCount
        var leftOverSpace
        if (leftOverIcons > 0) {
            leftOverSpace = shortcuts.height - shortcuts.itemHeight * maxopenItemsCount
            leftOverIcons = leftOverIcons - Math.floor(leftOverSpace / foldedSize)
            while (leftOverIcons > 0) {
                maxopenItemsCount--
                leftOverIcons = shortcuts.iconCount - maxopenItemsCount
                if (leftOverIcons > 0) {
                    leftOverSpace = shortcuts.height - shortcuts.itemHeight * maxopenItemsCount
                    leftOverIcons = leftOverIcons - Math.floor(leftOverSpace / foldedSize)
                }
            }
        }
        shortcuts.openItemsCount = Math.min(maxopenItemsCount, shortcuts.iconCount)
        shortcuts.visible = true
    }

    function createModels() {
        for (var i = 0; i < shortcutsModel.count; i++) {
            if (i < shortcuts.openItemsCount) {
                shortcuts.openModel.append(shortcutsModel.get(i))
            } else if (i == shortcuts.openItemsCount) {
                shortcuts.semiFoldedModel.append(shortcutsModel.get(i))
            } else {
                shortcuts.foldedModel.append(shortcutsModel.get(i))
            }
        }
    }

    function unfold() {
        if (!shortcuts.open) {
            var addvalue = 0
            if (shortcuts.currentIndex - shortcuts.openModel.count >= 0) {
                addvalue = -2 * shortcuts.itemHeight / 3 + shortcuts.fillItemHeight
            }
            if (shortcuts.currentIndex - shortcuts.openModel.count > 0) {
                addvalue += -2 * shortcuts.itemHeight / 3 * (shortcuts.currentIndex - shortcuts.openModel.count)
            }

            shortcutsContainer.targetY += addvalue
            shortcuts.open = true
        }
    }

    function fold() {
        flickable.contentY = 0
        shortcutsContainer.targetY = 0
        shortcuts.open = false
    }

    function scroll() {
        if (!scrolling && shortcutsContainer.height > shortcuts.height) {
            var scrollingAtStart = shortcutsContainer.targetY == 0
            var scrollingAtEnd = shortcutsContainer.targetY == shortcuts.height - shortcutsContainer.height
            var scrollingInTheMiddle = shortcutsContainer.targetY > (shortcuts.height - shortcutsContainer.height) && shortcutsContainer.targetY < 0
            var scrollValue = inverted ? -shortcuts.scrollValue : shortcuts.scrollValue

            shortcutsContainer.animateY = false

            if (scrollingInTheMiddle || (scrollingAtEnd && scrollValue > 0) || (scrollingAtStart && scrollValue < 0)) {
                scrollTimer.start()
            }
        }
    }

    function stopScroll() {
        scrollTimer.stop()
        shortcutsContainer.animateY = true
    }

    function calculateHighlight() {
        var posX = releaseModeMouse.x
        var posY = inverted ? shortcuts.height - releaseModeMouse.y : releaseModeMouse.y
        var container = shortcutsContainer.childAt(shortcutsContainer.width/2, posY - shortcutsContainer.y)
        var targetObject
        if (container) {
            var posObject = shortcuts.mapToItem(container, container.width/2, posY)
            targetObject = container.childAt(posObject.x, posObject.y)
            if (targetObject) {
                if (targetObject.ownIndex !== undefined) {
                    shortcuts.currentIndex = targetObject.ownIndex
                }
            }
        } else {
            shortcuts.resetHighlight()
        }
    }

    function resetHighlight() {
        shortcuts.currentIndex = -1
    }

    function selectCurrentApplication() {
        if (currentIndex < shortcuts.openItemsCount || foldedShortcuts.foldedAngle == 0) {
            return currentIndex
        } else {
            return -1
        }
    }

    /* If user has scrolled in release mode and enters the ribbon mode we need to set
       flickable's contentY to be the same as what scrolled value (i.e. targetY) was before switch.
    */
    function switchToRibbonMode() {
        var storedValue = shortcutsContainer.animateY
        shortcutsContainer.animateY = false
        flickable.contentY = -shortcutsContainer.targetY
        shortcutsContainer.targetY = 0
        shortcutsContainer.animateY = storedValue
    }

    BorderImage {
        id: background
        source: "graphics/launcher_bg.sci"
        width: parent.width
        x: 0
        rotation: inverted ? 180 : 0
        height: parent.height
    }

    Component {
        id: shortcutsItemDelegate
        Item {
            id: delegateItem
            property int ownIndex: parent ? index + parent.initialIndex : -1
            property bool isCurrentItem: ownIndex == shortcuts.currentIndex
            property bool ribbonModeHighlight: itemMouseArea.pressed

            z: -index
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            width: shortcuts.itemWidth
            height: shortcuts.itemHeight

            Item {
                width: shortcuts.iconWidth
                height: shortcuts.iconHeight
                anchors.centerIn: parent
                transform: Rotation { origin.x: width/2; origin.y: height; axis { x: 1; y: 0; z: 0 } angle: parent ? parent.foldedAngle : 0 }
                smooth: true
                UbuntuShape {
                    id: icon
                    objectName: "delegate " + model.desktopFile
                    anchors.centerIn: parent
                    rotation: inverted ? 180 : 0
                    width: shortcuts.iconWidth
                    height: width * units.gu(7.5) / units.gu(8)
                    radius: "medium"
                    borderSource: ""
                    image: Image {
                        id: iconImage
                        source:  '../' + shortcuts.iconPath + "/" + model.icon + '.png'
                        sourceSize { width: icon.width; height: icon.height }
                    }
                }

                BorderImage {
                    id: overlayHighlight
                    anchors.centerIn: icon
                    rotation: inverted ? 180 : 0
                    source: isCurrentItem && (releaseModeHighlight || ribbonModeHighlight) ? "graphics/selected.sci" : "graphics/non-selected.sci"
                    width: icon.width + units.gu(1.5)
                    height: width * units.gu(7.5) / units.gu(8)
                }
            }
            MouseArea {
                id: itemMouseArea
                anchors.fill: parent
                onPressed: shortcuts.currentIndex = ownIndex
                onClicked: {
                    if (model.desktopFile === "dash.desktop") shortcuts.dashItemSelected(ownIndex)
                    else shortcuts.applicationSelected(model.desktopFile)
                }
            }
        }
    }

    Flickable {
        id: flickable
        width: parent.width
        height: parent.height
        contentWidth: width
        contentHeight: shortcutsContainer.height
        Item {
            id: shortcutsContainer

            property bool animateY: true
            property real targetY

            x: ( parent.width - background.width) / 2
            y: targetY
            width: shortcuts.width
            height: openShortcuts.height + semiFoldedShortcut.height + foldedShortcuts.height

            Behavior on y { NumberAnimation{duration: shortcutsContainer.animateY ? 200 : 0; } }

            onYChanged: {
                if (scrolling) {
                    calculateHighlight()
                }
            }

            ShortcutsContainer {
                id: openShortcuts
                objectName: "openShortcuts"

                width: shortcuts.width
                height: childrenRect.height
                z: 3
                model: shortcuts.openModel
                delegate: shortcutsItemDelegate
                initialIndex: 0
            }

            ShortcutsContainer {
                id: semiFoldedShortcut
                objectName: "semiFoldedShortcut"

                foldedAngle: 0 // FIXME: folding disabled // shortcuts.open ? 0 : 90 * (height - initialFoldedOffset)/height - 80 // todo: better algorithm here
                initialFoldedOffset: -2 * shortcuts.itemHeight / 3 + shortcuts.fillItemHeight
                foldedOffset: 0 // FIXME: folding disabled //shortcuts.open ? 0 : initialFoldedOffset
                visible: shortcuts.iconCount > shortcuts.openItemsCount
                width: shortcuts.itemWidth
                height: childrenRect.height
                anchors.top: openShortcuts.bottom
                z: 2
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: foldedOffset
                model: shortcuts.semiFoldedModel
                delegate: shortcutsItemDelegate
                initialIndex: shortcuts.openModel.count
            }

            ShortcutsContainer {
                id: foldedShortcuts
                objectName: "foldedShortcuts"

                foldedAngle: 0 // FIXME: folding disabled //shortcuts.open ? 0 : 68
                initialFoldedOffset: 2 * shortcuts.itemHeight / 3
                foldedOffset: 0 // FIXME: folding disabled // shortcuts.open ? 0 : initialFoldedOffset
                width: shortcuts.width
                height: childrenRect.height
                spacing: -foldedOffset
                anchors.top: semiFoldedShortcut.bottom
                anchors.topMargin: -foldedOffset
                z: 1
                model: shortcuts.foldedModel
                delegate: shortcutsItemDelegate
                initialIndex: shortcuts.openModel.count + shortcuts.semiFoldedModel.count
            }
        }
    }

    Timer {
        id: scrollTimer

        repeat: true
        running: false
        interval: 16
        onTriggered:{
            if (shortcutsContainer.height > shortcuts.height) {
                var scrollValue = inverted ? -shortcuts.scrollValue : shortcuts.scrollValue
                var newValue = shortcutsContainer.targetY + scrollValue

                if (newValue > 0) {
                    newValue = 0
                    stop()
                } else if (newValue < shortcuts.height - shortcutsContainer.height) {
                    newValue = shortcuts.height - shortcutsContainer.height
                    stop()
                }
                shortcutsContainer.targetY = newValue
            }
        }
    }
}
