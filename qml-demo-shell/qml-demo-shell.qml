import QtQuick 2.0
import Unity.Application 0.1

Rectangle {
    id: root
    gradient: Gradient {
        GradientStop { position: 0.0; color: "lightsteelblue" }
        GradientStop { position: 1.0; color: "pink" }
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
            target: closeAnimation.surface.parent.parent
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
