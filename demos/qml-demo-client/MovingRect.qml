import QtQuick 2.0

Rectangle {
    id: movingRect
    width: 80
    height: 80
    color: "red"

    property real maxX: parent.width - width
    property real targetX: maxX

    function flipTargetX() {
        if (targetX == 0) {
            targetX = maxX
        } else if (targetX == maxX) {
            targetX = 0
        }
    }

    Timer {
        property real step: 4
        repeat: true
        running: true
        interval: 1000 / 60
        onTriggered: {
            if (x < targetX) {
                if (x + step > targetX) {
                    x = targetX;
                } else {
                    x += step;
                }
            } else if (x > targetX) {
                if (x - step < targetX) {
                    x = targetX;
                } else {
                    x -= step;
                }
            }
        }
    }

    onXChanged: {
        if (x == targetX) {
            flipTargetX();
        }
    }

    MouseArea {
        anchors.fill: parent
        onPressed: {
            parent.flipTargetX();
        }
    }
}
