import QtQuick 2.0
import Mir.Application 0.1

Rectangle {
    id: root
    gradient: Gradient {
        GradientStop { position: 0.0; color: "lightsteelblue" }
        GradientStop { position: 1.0; color: "pink" }
    }

    MultiPointTouchArea { //TODO: PinchArea might be more suitable
        anchors.fill: parent
        touchPoints: [
            TouchPoint { id: point1 },
            TouchPoint { id: point2 },
            TouchPoint { id: point3 }
        ]
        minimumTouchPoints: 3
        property Item window: null
        property real initialX: 0
        property real initialY: 0
        property real initialRotation: 0
        property real initialScale: 1
        property real touchInitialMidPointX: 0
        property real touchInitialMidPointY: 0
        property real touchInitialRotation: 0
        property real touchInitialScale: 0

        onPressed: {
            // if at least 2 touch points are within a Window, select that Window
            var windowUnderPoint1 = windowContainer.childAt(point1.x, point1.y); // not correct with Qt5.0.1 or less
            // see https://bugreports.qt-project.org/browse/QTBUG-28706
            if (!windowUnderPoint1) return;
            var windowUnderPoint2 = windowContainer.childAt(point2.x, point2.y);
            if (!windowUnderPoint2) return;
            var windowUnderPoint3 = windowContainer.childAt(point3.x, point3.y);
            if (!windowUnderPoint3) return;
            if (windowUnderPoint1 == windowUnderPoint2 || windowUnderPoint1 == windowUnderPoint3) {
                window = windowUnderPoint1;
            } else if (windowUnderPoint2 == windowUnderPoint3){
                window = windowUnderPoint2;
            } else {
                return;
            }

            // grab initial properties of the selected Window
            initialX = window.x
            initialY = window.y
            initialRotation = window.rotation
            initialScale = window.scale

            // calculate Euclidian distance between 2 of the points, the slope of this line and
            // the mid point coordinates
            touchInitialMidPointX = (point2.x + point1.x) / 2;
            touchInitialMidPointY = (point2.y + point1.y) / 2;
            touchInitialRotation = Math.atan((point2.y - point1.y) / (point2.x - point1.x)) * 180 / Math.PI;
            touchInitialScale = Math.sqrt(Math.pow(point2.x - point1.x, 2) + Math.pow(point2.y - point1.y, 2)) / 2;
        }
        onReleased: {
            window = null
        }
        onUpdated: {
            if (!window) return;

            // calculate Euclidian distance between 2 of the points, the slope of this line and
            // the mid point coordinates
            var x = (point2.x + point1.x) / 2;
            var y = (point2.y + point1.y) / 2;
            var d = Math.sqrt(Math.pow(point2.x - point1.x, 2) + Math.pow(point2.y - point1.y, 2)) / 2;
            var theta = (point2.y - point1.y) / (point2.x - point1.x);

            window.x = initialX + x - touchInitialMidPointX;
            window.y = initialY + y - touchInitialMidPointY;
            //window.rotation = initialRotation + (Math.atan(theta) * 180 / Math.PI) - touchInitialRotation;
            // disabling as causes Qt5.2 renderer crash with error: ASSERT: "tl.y <= br.y" in file scenegraph/coreapi/qsgbatchrenderer_p.h, line 108
            //window.scale = initialScale + (d / touchInitialScale - 1); // this is linear scaling
            window.scale = initialScale + Math.log(d / touchInitialScale) * Math.sqrt(Math.abs(d / touchInitialScale));
        }

        Item {
            id: windowContainer
            anchors.fill: parent

            Rectangle { width: 300; height: 400; color: "blue"; }
        }
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

    Rectangle {
        width: 30; height: 30
        color: "red"
        x: point3.x
        y: point3.y
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
        from: root.width; to: 0;
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
                closeAnimation.surface.parent.parent.destroy();
                closeAnimation.surface.release();
            }
        }
    }
}
