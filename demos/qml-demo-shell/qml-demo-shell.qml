import QtQuick 2.0
import Mir.Application 0.1

Rectangle {
    id: root

    focus: true
    Keys.onVolumeUpPressed: {
        console.log("\"Volume Up\" pressed");
    }
    Keys.onVolumeDownPressed: {
        console.log("\"Volume Down\" pressed");
    }

    gradient: Gradient {
        GradientStop { position: 0.0; color: "lightsteelblue" }
        GradientStop { position: 1.0; color: "pink" }
    }

    Image {
        id: unityLogo
        source: "UnityLogo.png"
        fillMode: Image.PreserveAspectFit
        anchors.centerIn: parent
        width: 600
        height: 600

        RotationAnimation {
            id: logoAnimation
            target: unityLogo
            from: 0
            to: 359
            duration: 3000
            easing.type: Easing.Linear
            loops: Animation.Infinite
        }

    }

    MultiPointTouchArea {
        anchors.fill: parent
        minimumTouchPoints: 1
        maximumTouchPoints: 1
        touchPoints: [
            TouchPoint { id: point }
        ]
        property Item window: null
        property real previousX: 0
        property real previousY: 0

        onPressed: {
            // if at least 2 touch points are within a Window, select that Window
            window = windowContainer.childAt(point.x, point.y);

            // save mouse position
            previousX = point.x
            previousY = point.y
        }

        onUpdated: {
            if (!window) return;

            var offset = point.x - previousX
            if (window.width > 100) {
                window.width += offset
            }
            previousX = point.x

            offset = point.y - previousY
            if (window.height > 100) {
                window.height += offset
            }
            previousY = point.y
        }

        onReleased: {
            window = null
            print(window.width, window.height)
        }

        MultiPointTouchArea {
            id: touchArea
            x: unityLogo.x
            y: unityLogo.y
            width: unityLogo.width
            height: unityLogo.height
            minimumTouchPoints:1
            maximumTouchPoints:1
            onPressed: {
                if (logoAnimation.paused) {
                    logoAnimation.resume();
                } else if (logoAnimation.running) {
                    logoAnimation.pause();
                } else {
                    logoAnimation.start();
                }
            }
        }

        Item {
            id: windowContainer
            anchors.fill: parent
        }
    }

    Rectangle {
        width: 30; height: 30
        color: "green"
        x: point.x
        y: point.y
    }

    Connections {
        target: SurfaceManager
        onSurfaceCreated: {
            print("new surface", surface.name)

            var windowComponent = Qt.createComponent("Window.qml");
            var window = windowComponent.createObject(windowContainer);
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
        from: root.width; to: 10;
        duration: 1200; easing.type: Easing.InOutQuad
    }

    SequentialAnimation {
        id: closeAnimation
        property variant surface: null
        NumberAnimation {
            target: (closeAnimation.surface && closeAnimation.surface.parent) ? closeAnimation.surface.parent.parent : null
            property: "scale";
            to: 0;
            duration: 500; easing.type: Easing.InQuad
        }
        ScriptAction {
            script: {
                closeAnimation.surface.parent.destroy(); //parent.destroy();
                closeAnimation.surface.release();
                print("surface destroyed")
            }
        }
    }
}
