import QtQuick 2.0

Rectangle {
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
}
