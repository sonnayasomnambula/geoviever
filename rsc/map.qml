import QtGraphicalEffects 1.15
import QtQuick 2.0
import QtLocation 5.6
import QtPositioning 5.6
import QtQuick.Shapes 1.1

Rectangle {
    Map {
        id: map
        anchors.fill: parent
        plugin: Plugin { name: "osm"; }
        center:  QtPositioning.coordinate(59.91, 10.75) // Oslo
        zoomLevel: 5

        CoordinateAnimation {
            id: centerAnimation;
            target: map;
            properties: "center";
            duration: 1200
            easing.type: Easing.InOutQuad
        }

        PropertyAnimation {
            id: zoomAnimation;
            target: map;
            properties: "zoomLevel"
            duration: 1200
            easing.type: Easing.InOutQuad
        }

        Connections {
            target: controller
            function onCenterChanged() {
                // console.log("onCenterChanged...", map.center.latitude, map.center.longitude, "==>", controller.center.latitude, controller.center.longitude)
                if (Math.abs(controller.center.latitude - map.center.latitude) > Number.EPSILON &&
                    Math.abs(controller.center.longitude - map.center.longitude) > Number.EPSILON) {
                    centerAnimation.from = map.center
                    centerAnimation.to = controller.center
                    centerAnimation.start()
                    // console.log("onCenterChanged", controller.center.latitude, controller.center.longitude)
                    // map.center = controller.center
                }
            }
            function onZoomChanged() {
                if (Math.abs(controller.zoom - map.zoomLevel) > Number.EPSILON) {
                    // map.zoomLevel = controller.zoom
                    zoomAnimation.from = map.zoomLevel
                    zoomAnimation.to = controller.zoom
                    zoomAnimation.start()
                }
            }
        }

        MapItemView {
            model: controller
            delegate: MapQuickItem {
                coordinate: QtPositioning.coordinate(_latitude_, _longitude_)
                anchorPoint: Qt.point(thumbnail.width * 0.5, thumbnail.height * 0.5)
                sourceItem: Shape {
                    id: thumbnail
                    width: 32
                    height: 32
                    visible: true

                    Image {
                        id: pic
                        source: _pixmap_
                    }

                    ColorOverlay {
                        id: overlay
                        anchors.fill: pic
                        source: pic
                        color: controller.currentRow === _index_ ? "#40000040" : "#00000000"
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: controller.currentRow = _index_;
                        onEntered: controller.hoveredRow = _index_;
                        onExited: controller.hoveredRow = -1;
                    }
                }
            }
        }

        onZoomLevelChanged: controller.zoom = map.zoomLevel
        onCenterChanged: controller.center = map.center
    }
}
