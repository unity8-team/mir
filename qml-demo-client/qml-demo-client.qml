import QtQuick 2.0

Rectangle {
    onWidthChanged: print("WIDTH", width)
    onHeightChanged: print("HEIGHT", height)
    width: 600
    height: 80*6
    color: "blue"

    Column {
        anchors.fill: parent
        MovingRect {}
        MovingRect {}
        MovingRect {}
        MovingRect {}
        MovingRect {}
        MovingRect {}
    }

    Rectangle {
        id: flasher
        width: 100
        height: 100
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        color: (on) ? "yellow" : "green"
        property bool on: false
        Timer {
            interval: 1000
            repeat: true
            running: true
            onTriggered: flasher.on = !flasher.on
        }
    }
}
