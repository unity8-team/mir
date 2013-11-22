import QtQuick 2.0
import Unity.Application 0.1

Rectangle {
    id: root
    color: "blue"

    Connections {
        target: SurfaceManager
        onSurfaceCreated: {
            print("new surface", surface.name)
            surface.parent = root;
surface.height = surface.height / 2;
surface.width = surface.width * 2;
            openAnimation.target = surface;
            openAnimation.start();
        }

        onSurfaceDestroyed: {
            print("surface destroyed", surface.name)
            surface.parent = null;
            delete surface;
        }
    }

    NumberAnimation {
        id: openAnimation
        property: "x";
        from: root.width; to: 0;
        duration: 1200; easing.type: Easing.InOutQuad
    }
}
