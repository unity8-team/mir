import QtQuick 2.0

Rectangle {
    id: root
    color: "red"
    readonly property int margin: 5

    function setSurface(surface) {
        surface.parent = root //windowContainer;
        root.width = surface.width //+ 2*margin;
        root.height = surface.height //+ margin + title.height;

        surface.anchors.fill = root //windowContainer;
//        surface.opacity = 0.99;

//        title.text = surface.name;
    }

//    Text {
//        id: title
//        anchors {
//            left: parent.left
//            right: parent.right
//            top: parent.top
//            leftMargin: margin
//            rightMargin: margin
//        }
//        font.pixelSize: 40
//    }

    Rectangle {
        id: windowContainer
        anchors {
            left: parent.left
            right: parent.right
            top: parent.top
//            top: title.bottom
            bottom: parent.bottom
            leftMargin: margin
            rightMargin: margin
            bottomMargin: margin
        }
        color: "pink"
        opacity: 0.2
    }
}
