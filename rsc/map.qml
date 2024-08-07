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
        center:  QtPositioning.coordinate(59.95, 30.32)
        zoomLevel: 11

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
                if (Math.abs(controller.center.latitude - map.center.latitude) > Number.EPSILON &&
                    Math.abs(controller.center.longitude - map.center.longitude) > Number.EPSILON) {
                    centerAnimation.from = map.center
                    centerAnimation.to = controller.center
                    centerAnimation.start()
                }
            }
            function onZoomChanged() {
                if (Math.abs(controller.zoom - map.zoomLevel) > Number.EPSILON) {
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
                    width: controller.thumbnailSize
                    height: controller.thumbnailSize
                    visible: true

                    Image {
                        id: pic
                        source: _pixmap_
                    }

                    ColorOverlay {
                        id: overlay
                        anchors.fill: pic
                        source: pic
                        color: selection.currentIndex.row === index ? "#40000040" : "#00000000"
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true

                        onClicked: selection.currentRow = index;
                        onEntered: selection.hoveredRow = index;
                        onExited: selection.hoveredRow = -1;
                    }
                }
            }
        }

        onZoomLevelChanged: controller.zoom = map.zoomLevel
        onCenterChanged: controller.center = map.center
    }
}
