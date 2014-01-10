import QtQuick 2.0

Rectangle {
    id: root
    color: "red"
    readonly property int margin: 5

    function setSurface(surface) {
        surface.parent = windowContainer;
        root.width = surface.width + 2*margin;
        root.height = surface.height + margin + title.height;

        surface.anchors.fill = windowContainer;

        title.text = surface.name;
    }

    Text {
        id: title
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
            leftMargin: margin
            rightMargin: margin
        }
        font.pixelSize: 40
    }

    Rectangle {
        id: windowContainer
        opacity: 0.999 // FIXME: something weird has happened
        anchors {
            left: parent.left
            right: parent.right
            top: title.bottom
            bottom: parent.bottom
            leftMargin: margin
            rightMargin: margin
            bottomMargin: margin
        }
    }
}
