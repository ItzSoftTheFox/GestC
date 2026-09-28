import QtQuick
Rectangle {
    id: panel
    property real tintOpacity: 0.10
    radius: 22
    color: Qt.rgba(1, 1, 1, tintOpacity)
    border.width: 1
    border.color: "#22ffffff"
    Rectangle {
        x: parent.radius; y: 0
        width: Math.max(0, parent.width - parent.radius * 2); height: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0; color: "transparent" }
            GradientStop { position: 0.5; color: "#70ffffff" }
            GradientStop { position: 1; color: "transparent" }
        }
    }
}
