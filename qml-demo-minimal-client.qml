import QtQuick 2.0

Rectangle {
    width: 600
    height: 600
    color: "grey"

    Rectangle {
        id: rect
        anchors.centerIn: parent
        color: "green"
        width: 400
        height: 400

        RotationAnimation {
            target: rect
            from: 0
            to: 359
            duration: 3000
            easing.type: Easing.Linear
            loops: Animation.Infinite
            running: true
        }
    }
}
