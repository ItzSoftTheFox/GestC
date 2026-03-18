import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import CustomElements 1.0

ApplicationWindow {
    visible: true
    width: 1050
    height: 650
    // Prevent the user from shrinking the window too much
    minimumWidth: 850
    minimumHeight: 550
    title: "HandMouse Pro"
    
    // Modern background with a subtle gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0f172a" }
            GradientStop { position: 1.0; color: "#1e1b4b" }
        }
    }

    Shortcut {
        sequence: "Esc"
        onActivated: scannerBackend.stopCamera()
    }
    
    Shortcut {
        sequence: "Ctrl+Q"
        onActivated: Qt.quit()
    }

    Shortcut {
        sequence: "P"
        onActivated: scannerBackend.togglePause()
    }

    // Main responsive grid container filling the entire window
    RowLayout {
        anchors.fill: parent
        anchors.margins: 30
        spacing: 30

        // Left section (Video and Controls) grows dynamically
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 25

            // Flexible glass frame for the camera feed
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: Qt.rgba(1, 1, 1, 0.03)
                border.color: Qt.rgba(1, 1, 1, 0.1)
                border.width: 1
                radius: 8

                // Wrapper item to perfectly align the video and the overlay on top of each other
                Item {
                    anchors.fill: parent
                    anchors.margins: 10

                    VideoItem {
                        anchors.fill: parent
                        scanner: scannerBackend
                    }

                    // Dark overlay indicating the paused state
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 10
                        color: Qt.rgba(0, 0, 0, 0.7)
                        radius: 6
                        // Require BOTH the system to be paused AND the camera to be actively scanning
                        visible: scannerBackend.isPaused && scannerBackend.isScanning
                        
                        Text {
                            anchors.centerIn: parent
                            text: "PAUZNUTO"
                            color: "white"
                            font.pixelSize: 42
                            font.bold: true
                            font.letterSpacing: 6
                        }
                    }
                }
            }

            // Control bar centered at the bottom
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 20

                Button {
                    text: "Start"
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 40
                    onClicked: scannerBackend.startCamera()
                    background: Rectangle {
                        color: parent.pressed ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                        border.color: Qt.rgba(1, 1, 1, 0.15)
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                Button {
                    text: "Stop"
                    Layout.preferredWidth: 120
                    Layout.preferredHeight: 40
                    onClicked: scannerBackend.stopCamera()
                    background: Rectangle {
                        color: parent.pressed ? Qt.rgba(1, 1, 1, 0.1) : Qt.rgba(1, 1, 1, 0.05)
                        border.color: Qt.rgba(1, 1, 1, 0.15)
                        radius: 6
                    }
                    contentItem: Text {
                        text: parent.text
                        color: "white"
                        font.pixelSize: 15
                        font.bold: true
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // Status indicator
                Rectangle {
                    Layout.preferredWidth: 140
                    Layout.preferredHeight: 40
                    radius: 6
                    color: Qt.rgba(0, 0, 0, 0.2)
                    border.color: Qt.rgba(1, 1, 1, 0.05)
                    
                    Row {
                        anchors.centerIn: parent
                        spacing: 10
                        Rectangle {
                            width: 12
                            height: 12
                            radius: 6
                            anchors.verticalCenter: parent.verticalCenter
                            color: scannerBackend.isScanning ? "#32d74b" : "#ff453a"
                            Behavior on color { ColorAnimation { duration: 300 } }
                            
                            Rectangle {
                                anchors.centerIn: parent
                                width: 20
                                height: 20
                                radius: 10
                                color: parent.color
                                opacity: 0.3
                            }
                        }
                        Text {
                            text: scannerBackend.isScanning ? "Aktivní" : "Zastaveno"
                            color: "white"
                            font.pixelSize: 14
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        // Right glass panel for calibration retains fixed width but fills height
        Rectangle {
            Layout.preferredWidth: 320
            Layout.fillHeight: true
            
            color: Qt.rgba(1, 1, 1, 0.04)
            border.color: Qt.rgba(1, 1, 1, 0.12)
            border.width: 1
            radius: 8

            Column {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 20

                Text { 
                    text: "Kalibrace Modelu" 
                    color: "white" 
                    font.pixelSize: 20
                    font.bold: true
                    bottomPadding: 10
                }

                RowLayout {
                    width: parent.width
                    Text { 
                        text: "GPU Akcelerace (OpenCL)" 
                        color: "#a1a1aa" 
                        font.pixelSize: 13 
                    }
                    Item { Layout.fillWidth: true }
                    Switch {
                        checked: scannerBackend.useGPU
                        onCheckedChanged: scannerBackend.useGPU = checked
                    }
                }

                component GlassSlider: Column {
                    property string labelText
                    property alias value: internalSlider.value
                    property alias from: internalSlider.from
                    property alias to: internalSlider.to
                    property int decimals: 2
                    
                    width: parent.width
                    spacing: 8

                    RowLayout {
                        width: parent.width
                        Text { text: labelText; color: "#a1a1aa"; font.pixelSize: 13 }
                        Item { Layout.fillWidth: true }
                        Text { text: internalSlider.value.toFixed(decimals); color: "white"; font.pixelSize: 13; font.bold: true }
                    }

                    Slider {
                        id: internalSlider
                        width: parent.width
                        
                        background: Rectangle {
                            x: internalSlider.leftPadding
                            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
                            implicitWidth: 200
                            implicitHeight: 6
                            width: internalSlider.availableWidth
                            height: implicitHeight
                            radius: 3
                            color: Qt.rgba(1, 1, 1, 0.1)

                            Rectangle {
                                width: internalSlider.visualPosition * parent.width
                                height: parent.height
                                color: "#0a84ff"
                                radius: 3
                            }
                        }
                        
                        handle: Rectangle {
                            x: internalSlider.leftPadding + internalSlider.visualPosition * (internalSlider.availableWidth - width)
                            y: internalSlider.topPadding + internalSlider.availableHeight / 2 - height / 2
                            implicitWidth: 16
                            implicitHeight: 16
                            radius: 4
                            color: internalSlider.pressed ? "#d1d5db" : "#ffffff"
                            border.color: Qt.rgba(0, 0, 0, 0.1)
                            border.width: 1
                        }
                    }
                }

                GlassSlider {
                    labelText: "Osa X (Posun)"
                    from: -0.2; to: 0.2
                    value: scannerBackend.offsetX
                    onValueChanged: scannerBackend.offsetX = value
                }

                GlassSlider {
                    labelText: "Osa Y (Posun)"
                    from: -0.2; to: 0.2
                    value: scannerBackend.offsetY
                    onValueChanged: scannerBackend.offsetY = value
                }

                GlassSlider {
                    labelText: "Velikost výřezu"
                    from: 1.0; to: 3.5
                    value: scannerBackend.cropMultiplier
                    onValueChanged: scannerBackend.cropMultiplier = value
                }

                GlassSlider {
                    labelText: "Citlivost pohybu"
                    from: 1.0; to: 4.0
                    value: scannerBackend.movementScale
                    onValueChanged: scannerBackend.movementScale = value
                }

                GlassSlider {
                    labelText: "Vyhlazování pohybu"
                    from: 0.05; to: 1.0
                    value: scannerBackend.smoothingFactor
                    onValueChanged: scannerBackend.smoothingFactor = value
                }

                GlassSlider {
                    labelText: "Práh kliknutí"
                    from: 0.05; to: 0.30
                    decimals: 3
                    value: scannerBackend.clickThreshold
                    onValueChanged: scannerBackend.clickThreshold = value
                }
                
                GlassSlider {
                    labelText: "Rychlost scrollování"
                    // 5.0 is slow precision scrolling. 30.0 is extremely fast.
                    from: 5.0; to: 30.0
                    decimals: 1
                    value: scannerBackend.scrollSensitivity
                    onValueChanged: scannerBackend.scrollSensitivity = value
                }
            }
        }
    }
}