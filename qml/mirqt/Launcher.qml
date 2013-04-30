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
import "../Components"
import Ubuntu.Components 0.1
import "../Applications/applications.js" as ApplicationsModel

Item {
    id: launcher

    property var favourites: ["/usr/share/applications/phone-app.desktop",
                              "/usr/share/applications/camera-app.desktop",
                              "/usr/share/applications/gallery-app.desktop",
                              "/usr/share/applications/facebook-webapp.desktop",
                              "/usr/share/applications/webbrowser-app.desktop",
                              "/usr/share/applications/twitter-webapp.desktop",
                              "/usr/share/applications/gmail-webapp.desktop",
                              "/usr/share/applications/music-player-mockapp.desktop",
                              "/usr/share/applications/notes-app.desktop",
                              "/usr/share/applications/ski-safari-mockapp.desktop",
                              "/usr/share/applications/media-player.desktop"]
    property ListModel model: ListModel {}
    property ListModel applicationsRunning: ListModel {}
    property ListModel gadgetsRunning: ListModel {}
    property int lastFavouriteIndex: -1

    property bool gadgetsEnabled: false
    property bool available: true // can be used to disable all interactions
    property bool applicationFocused: false

    property bool shown: launcher.state != ""
    property int progress
    progress: {
        if (launcher.state == "spreadMoving")
            return mouseArea.mouseX
        else if (launcher.state == "spread")
            return launcher.width
        else
            return 0
    }

    // Thresholds and dimensions
    property int shortcutsHintWidth: units.gu(1)
    property int spreadHintWidth: units.gu(3)
    property int shortcutsWidth: units.gu(6)
    property int shortcutsThreshold: units.dp(2)
    property int shortcutsCloseThreshold: units.gu(1)
    property int spreadHintThreshold: shortcutsWidth
    property int spreadOutThreshold: units.gu(18)
    property int gadgetsSpreadWidth: units.gu(11)

    // If the user flicks with a speed greater than this, the highlighted
    // application won't be launched.
    // Rationale (my guess) is to avoid accidental app launches as it was likely
    // too fast for the user to actually see what icon got selected.
    // In pixels/second
    property real maxFlickSpeedToLaunchApp: units.gu(10)

    // For how long (in milliseconds) the launcher will stay up without being
    // interacted with, before it finally hides itself.
    property int inactivityTimeout: 8000

    property string iconPath: "."
    property string appScreenshotPath: "."
    property string gadgetScreenshotPath: "."

    property int launcherProgressValue
    launcherProgressValue: {
        if (launcher.state == ""
                || launcher.state == "hint"
                || launcher.state == "moving"
                || launcher.state == "shortcuts") {
            return 0
        } else if (launcher.state == "spreadHint") {
            return units.gu(17)
        } else if (launcher.state == "spreadMoving") {
            return units.gu(22)
        } else if (launcher.state == "spread") {
            return units.gu(28)
        }
    }

    signal launcherItemSelected(int itemIndex)
    signal launcherApplicationSelected(string name)
    signal launcherGadgetSelected(string name)
    signal dashItemSelected(int itemIndex)
    signal applicationClosed(int index, string name, string type)
    signal allClosed()

    signal moving()
    signal dash()
    signal begin()

    // Can be replaced with a fake implementation during tests
    property var __dateTime: new function() {
        this.getCurrentTimeMs = function() {return new Date().getTime()}
    }

    // internal properties hidden from public api
    QtObject {
        id: priv

        property bool shortcutAnimationOngoing
        property string previousStateHelp
        property string previousState: ""
    }

    onStateChanged: {
        priv.previousState = priv.previousStateHelp
        priv.previousStateHelp = state
        if (launcher.state != "shorcuts") {
            dismissTimer.stop()
        }

        if (launcher.state == "spread") {
            mouseArea.visible = false
            hideTimer.restart()
            mouseArea.visible = true
        }
        //console.log("Launcher state: " + state)
    }

    Component.onCompleted: {
        __populateWithFavouriteApplications();
    }

    function __populateWithFavouriteApplications() {
        for (var i=0; i<launcher.favourites.length; i++) {
            var desktopFile = launcher.favourites[i];
            var application = ApplicationsModel.__availableApplications[desktopFile];
            if (application) {
                application["desktopFile"] = desktopFile;
                launcher.model.append(application);
            }
        }
    }

    function hide() {
        launcher.state = ""
    }

    function calculateCurrentShortcut() {
        var shortcutObject = mapToItem(shortcuts, mouseArea.mouseX, mouseArea.mouseY)
        shortcuts.calculateHighlight(shortcutObject.x, shortcutObject.y)
    }

    Rectangle {
        id: backgroundShade
        anchors.fill: parent
        color: "black"
        opacity: {
            if (launcher.state == "" || launcher.state == "hint" || launcher.state == "moving") {
                return 0
            } else return 0.4
        }
        visible: opacity != 0

        Behavior on opacity { NumberAnimation { duration: 200; easing.type: Easing.OutQuad} }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                launcher.state = ""
            }
            visible: launcher.state != "spread"
        }
    }

    LauncherShortcuts {
        id: shortcuts
        objectName: "shortcuts"
        width: shortcutsWidth
        anchors {
            top: parent.top
            bottom: parent.bottom
        }
        x: -width
        model: launcher.model
        onApplicationSelected: {
            launcher.state = ""
            launcherApplicationSelected(desktopFile)
        }
        onDashItemSelected: {
            launcher.state = ""
            launcher.dashItemSelected(index)
        }
        releaseModeHighlight: mouseArea.pressed && (launcher.state == "shortcuts" || launcher.state == "spreadHint")
        iconPath: launcher.iconPath
        releaseModeMouse: Qt.point(mouseArea.mouseX, mouseArea.mouseY)

        Behavior on x { NumberAnimation {duration: launcher.state == "hint" ? 70 : launcher.state == "moving" ? 0 : 300; easing.type: Easing.OutCubic } }
    }

    LauncherLabel {
        id: launcherLabel
        objectName: "launcherLabel"

        property int margin: units.gu(1)

        x: shortcutsWidth + margin
        y: {
            var offset = (shortcuts.iconHeight) * (shortcuts.labelPosition) / launcher.height
            return shortcuts.labelPosition - offset
        }
        width: Math.min(units.gu(40), launcher.width - launcher.shortcutsWidth - 2 * margin)
        height: shortcuts.iconHeight
        text: shortcuts.shortcutsModel.get(shortcuts.currentIndex) ? shortcuts.shortcutsModel.get(shortcuts.currentIndex).name : ""
        iconSource: shortcuts.shortcutsModel.get(shortcuts.currentIndex) ? '../' + iconPath + "/" + shortcuts.shortcutsModel.get(shortcuts.currentIndex).icon + '.png': ""
        opacity: shortcuts.currentIndex != -1 && mouseArea.pressed && (launcher.state == "shortcuts" || launcher.state == "spreadHint") ? 1 : 0

        Behavior on opacity {
            enabled: launcherLabel.opacity < 1.0
            SequentialAnimation {
                PauseAnimation { duration: 100 }
                NumberAnimation {duration: 100}
            }
        }
    }

    DraggingArea {
        id: mouseArea
        enabled: launcher.state != "shortcuts"
        property int initialX
        property int movingDelta: initialX - launcher.shortcutsHintWidth
        zeroVelocityCounts: true
        orientation: Qt.Horizontal
        visible: launcher.available
        width: {
            if (launcher.state == "") {
                return shortcutsThreshold
            } else {
                return shortcutsWidth
            }
        }
        height: launcher.height
        __dateTime: launcher.__dateTime

        onPressed: {
            initialX = mouseX
            if (launcher.state == "") launcher.state = "hint"
            dismissTimer.stop()

            if (launcher.state == "shortcuts" && !shortcuts.animating) {
                shortcuts.unfold()
            }
        }
        onPositionChanged: {
            if (launcher.state == "shortcuts" && mouseX < shortcutsCloseThreshold) {
                launcher.state = ""
            } else if (launcher.state == "hint" && mouseX > initialX ) {
                launcher.state = "moving"
            } else if (mouseX > shortcutsThreshold && mouseX < spreadHintThreshold) {
                launcher.state = "shortcuts"
            } else if (mouseX >= spreadHintThreshold && mouseX < shortcutsWidth) {
                if (launcher.applicationFocused){
                    launcher.state = "spreadHint"
                }
            } else if (mouseX >= shortcutsWidth && mouseX < spreadOutThreshold) {
                if (launcher.applicationFocused) {
                    launcher.state = "spreadMoving"
                } else {
                    dismissTimer.restart()
                }
            } else if (mouseX >= spreadOutThreshold && launcher.applicationFocused) {
                launcher.state = "spread"
            }
        }

        // launcher scrolling controls here
        onMouseYChanged: {
            var scrollAreaHeight = 3*shortcuts.itemHeight
            var maxScrollSpeed = units.gu(2)
            if (launcher.state == "shortcuts" && !priv.shortcutAnimationOngoing) {
                if (mouseY < scrollAreaHeight) {
                    shortcuts.scrollValue = maxScrollSpeed - mouseY / (scrollAreaHeight/maxScrollSpeed)
                    if (shortcuts.open && !shortcuts.animating) {
                        shortcuts.scroll()
                    }
                } else if (mouseArea.height - mouseY < scrollAreaHeight) {
                    shortcuts.scrollValue = -maxScrollSpeed + (mouseArea.height - mouseY) / (scrollAreaHeight/maxScrollSpeed)
                    if (shortcuts.open && !shortcuts.animating) {
                        shortcuts.scroll()
                    }
                } else {
                    shortcuts.stopScroll()
                }
            }
            if (!shortcuts.scrolling) shortcuts.calculateHighlight()
        }

        onReleased: {
            var currentApplicationIndex
            if (launcher.state == "shortcuts" && !priv.shortcutAnimationOngoing) {
                // launch app
                currentApplicationIndex = shortcuts.selectCurrentApplication()
                if (currentApplicationIndex === 0) {
                    launcher.dashItemSelected(0)
                    launcher.state = ""
                } else if (currentApplicationIndex === -1) {
                    launcher.state = "shortcuts"
                } else if (Math.abs(dragVelocity) < maxFlickSpeedToLaunchApp) {
                    launcherApplicationSelected(shortcuts.model.get(currentApplicationIndex).desktopFile)
                    launcher.state = ""
                }
            }

            if (launcher.state == "hint" || launcher.state == "moving") {
                launcher.state = ""
            } else if (launcher.state == "spreadMoving" || launcher.state == "spreadHint") {
                if (dragVelocity > units.gu(44)) {
                    launcher.state = "spread"
                } else {
                    shortcuts.switchToRibbonMode()
                    launcher.state = "shortcuts"
                }
            }

            shortcuts.resetHighlight()
            shortcuts.stopScroll()

            //unfoldTimer.stop()

            if (launcher.state == "shortcuts") {
                dismissTimer.restart()
            }

        }
    }

    Timer {
        id: dismissTimer
        interval: launcher.inactivityTimeout;
        running: false;
        repeat: false
        onTriggered: {
            launcher.state = ""
        }
    }

    Timer {
        id: hideTimer
        interval: 50;
        running: false;
        repeat: false
        onTriggered: {
            launcher.state = ""
        }
    }

    Timer {
        id: unfoldTimer
        interval: 150;
        running: false;
        repeat: false
        onTriggered: {
            shortcuts.unfold()
        }
    }

    states: [
        State {
            name: "hint"
            PropertyChanges {
                target: shortcuts
                x: -shortcuts.width + launcher.shortcutsHintWidth
            }
        },
        State {
            name: "moving"
            PropertyChanges {
                target: shortcuts
                x: -shortcuts.width + mouseArea.mouseX - mouseArea.movingDelta
            }
        },
        State {
            name: "shortcuts"
            PropertyChanges {
                target: shortcuts
                x: 0
            }
        },
        State {
            name: "spreadHint"
            extend: "shortcuts"
        },
        State {
            name: "spreadMoving"
            PropertyChanges {
                target: launcher
                launcherProgressValue: mouseArea.mouseX
            }
            PropertyChanges {
                target: shortcuts
                x: 0
            }
        },
        State {
            name: "spread"
            extend: "shortcuts"
        }

    ]

    transitions: [
        Transition {
            to: ""
            SequentialAnimation {
                PropertyAnimation {
                    target: shortcuts
                    properties: "x"
                    duration: 400
                    easing.type: Easing.OutCubic
                }
                ScriptAction {
                    script: {
                        shortcuts.fold()
                    }
                }
            }
        },
        Transition {
            to: "hint"
            PropertyAnimation {
                target: shortcuts
                properties: "x"
                duration: 100
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            to: "shortcuts"
            SequentialAnimation {
                ScriptAction {
                    script: {
                        if (priv.previousState == "moving") {
                            priv.shortcutAnimationOngoing = true
                        } else {
                            launcher.begin()
                        }

                        unfoldTimer.start()
                    }
                }
                ParallelAnimation {
                    SequentialAnimation {
                        PropertyAnimation {
                            target: shortcuts
                            properties: "x"
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                        ScriptAction {
                            script: {
                                priv.shortcutAnimationOngoing = false
                            }
                        }
                    }
                }
            }
        },
        Transition {
            to: "spreadHint"
            PropertyAnimation {
                target: shortcuts
                properties: "x"
                duration: 300
                easing.type: Easing.OutCubic
            }
        },
        Transition {
            to: "spreadMoving"
            ScriptAction {
                script: {
                    priv.shortcutAnimationOngoing = false
                    launcher.moving()
                }
            }
        },
        Transition {
            to: "spread"
            ScriptAction {
                script: {
                    launcher.dash()
                }
            }
        }
    ]
}
