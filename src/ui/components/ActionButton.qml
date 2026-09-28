import QtQuick
import QtQuick.Controls
Button {
    id: control
    property bool primary: false
    property bool compact: false
    implicitHeight: compact ? 36 : 44
    implicitWidth: Math.max(compact ? 72 : 110, label.implicitWidth + 32)
    hoverEnabled: true
    font.pixelSize: 13
    font.weight: Font.DemiBold
    Accessible.name: text
    background: Rectangle {
        radius: 12
        color: !control.enabled ? "#12ffffff" : control.primary
            ? (control.down ? "#92c5e9" : control.hovered ? "#d2edff" : "#b4deff")
            : (control.down ? "#30ffffff" : control.hovered ? "#20ffffff" : "#12ffffff")
        border.color: control.activeFocus ? "#b4deff" : "#22ffffff"
        border.width: control.activeFocus ? 2 : 1
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    contentItem: Text {
        id: label
        text: control.text
        font: control.font
        color: !control.enabled ? "#648093" : control.primary ? "#182c40" : "#eff5fc"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
