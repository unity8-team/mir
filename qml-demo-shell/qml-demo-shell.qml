import QtQuick 2.0
import Mir.Application 0.1

Rectangle {
    id: root
    gradient: Gradient {
        GradientStop { position: 0.0; color: "lightsteelblue" }
        GradientStop { position: 1.0; color: "pink" }
    }

    MultiPointTouchArea {
        anchors.fill: parent
        touchPoints: [
            TouchPoint { id: point1 },
            TouchPoint { id: point2 }
        ]
    }

    Rectangle {
        width: 30; height: 30
        color: "green"
        x: point1.x
        y: point1.y
    }

    Rectangle {
        width: 30; height: 30
        color: "yellow"
        x: point2.x
        y: point2.y
    }

    Connections {
        target: SurfaceManager
        onSurfaceCreated: {
            print("new surface", surface.name)

            var windowComponent = Qt.createComponent("Window.qml");
            var window = windowComponent.createObject(root);
            window.setSurface(surface);

            openAnimation.target = window;
            openAnimation.start();
        }

        onSurfaceDestroyed: {
            print("surface destroying", surface.name)
            closeAnimation.surface = surface;
            closeAnimation.start();
        }
    }

    NumberAnimation {
        id: openAnimation
        property: "x";
        from: root.width; to: 0;
        duration: 1200; easing.type: Easing.InOutQuad
    }

    SequentialAnimation {
        id: closeAnimation
        property variant surface: null
        NumberAnimation {
            target: (closeAnimation.surface && closeAnimation.surface.parent) ? closeAnimation.surface.parent.parent : null
            property: "scale";
            from: 1; to: 0;
            duration: 500; easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                closeAnimation.surface.parent.parent.destroy();
                closeAnimation.surface.release();
            }
        }
    }
}
