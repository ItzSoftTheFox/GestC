import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
RowLayout {
    id: row
    property string title
    property string description
    property bool checked: false
    signal toggled(bool newValue)
    spacing: 16
    ColumnLayout {
        Layout.fillWidth: true; spacing: 5
        Text { text: row.title; color: "#edf3fb"; font.pixelSize: 14 }
        Text { text: row.description; color: "#aebdce"; font.pixelSize: 12; wrapMode: Text.WordWrap; Layout.fillWidth: true }
    }
    Switch {
        id: toggle
        checked: row.checked
        onToggled: row.toggled(checked)
        Accessible.name: row.title
        indicator: Rectangle {
            implicitWidth: 42; implicitHeight: 24
            x: toggle.leftPadding; y: (toggle.height - height) / 2
            radius: 12; color: toggle.checked ? "#b4deff" : "#425369"
            border.width: toggle.activeFocus ? 2 : 0; border.color: "white"
            Rectangle {
                width: 18; height: 18; radius: 9; y: 3
                x: toggle.checked ? parent.width - width - 3 : 3
                color: toggle.checked ? "#203d55" : "#dae6f4"
                Behavior on x { NumberAnimation { duration: 140 } }
            }
        }
    }
}
