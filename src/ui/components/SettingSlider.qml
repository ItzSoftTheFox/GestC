import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
ColumnLayout {
    id: control
    property string title
    property string description
    property string displayValue
    property real value: 0
    property real from: 0
    property real to: 1
    property real stepSize: 0.01
    signal edited(real newValue)
    spacing: 8
    RowLayout {
        Layout.fillWidth: true
        Text { text: control.title; color: "#edf3fb"; font.pixelSize: 14; font.weight: Font.Medium }
        Item { Layout.fillWidth: true }
        Text { text: control.displayValue; color: "#b4deff"; font.pixelSize: 13; font.weight: Font.DemiBold }
    }
    Text {
        text: control.description; color: "#aebdce"; font.pixelSize: 12
        wrapMode: Text.WordWrap; Layout.fillWidth: true
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        implicitHeight: 28
        from: control.from; to: control.to; value: control.value; stepSize: control.stepSize
        onMoved: control.edited(value)
        Accessible.name: control.title
        background: Rectangle {
            x: slider.leftPadding; y: (slider.height - height) / 2
            width: slider.availableWidth; height: 5; radius: 3; color: "#30ffffff"
            Rectangle { width: parent.width * slider.visualPosition; height: parent.height; radius: 3; color: "#b4deff" }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2; width: 19; height: 19; radius: 10
            color: slider.pressed ? "#b4deff" : "#f3f8ff"
            border.width: slider.activeFocus ? 3 : 1; border.color: slider.activeFocus ? "#5f9ac5" : "#b8c9da"
        }
    }
}
