import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import HandMouse 1.0
import "components"

ApplicationWindow {
    id: window
    property int page: 0
    readonly property var controller: appController
    readonly property var settings: appSettings
    readonly property var preferences: settings.values
    visible: true
    width: 1160; height: 800
    minimumWidth: 920; minimumHeight: 660
    title: "HandMouse"
    color: "transparent"
    font.family: "Inter"
    font.pixelSize: 14
    background: Rectangle {
        radius: 22
        gradient: Gradient {
            GradientStop { position: 0; color: Qt.rgba(0.12, 0.18, 0.26, window.preferences.glassOpacity) }
            GradientStop { position: 0.55; color: Qt.rgba(0.10, 0.14, 0.22, window.preferences.glassOpacity) }
            GradientStop { position: 1; color: Qt.rgba(0.16, 0.14, 0.23, window.preferences.glassOpacity) }
        }
        border.width: 1; border.color: "#35ffffff"
    }
    Shortcut { sequence: "Escape"; onActivated: controller.stop() }
    Shortcut { sequence: "Space"; enabled: window.page !== 1; onActivated: controller.togglePause() }
    Shortcut { sequence: "Ctrl+,"; onActivated: window.page = 1 }
    Shortcut { sequence: "Ctrl+Q"; onActivated: window.close() }
    onClosing: controller.stop()

    RowLayout {
        anchors.fill: parent; anchors.margins: 20; spacing: 24
        // Translucent sidebar: the compositor supplies the actual background blur.
        GlassPanel {
            Layout.preferredWidth: 204; Layout.fillHeight: true; tintOpacity: 0.055
            ColumnLayout {
                anchors.fill: parent; anchors.margins: 18; spacing: 10
                Rectangle {
                    Layout.topMargin: 10; width: 44; height: 44; radius: 15
                    gradient: Gradient {
                        GradientStop { position: 0; color: "#d1ebff" }
                        GradientStop { position: 1; color: "#7ca3cf" }
                    }
                    Text { anchors.centerIn: parent; text: "H"; color: "#233b55"; font.pixelSize: 26; font.weight: Font.DemiBold }
                }
                Text { text: "HandMouse"; color: "#f0f6ff"; font.pixelSize: 23; font.weight: Font.DemiBold; Layout.topMargin: 6 }
                Text { text: "Tvá ruka. Tvůj kurzor."; color: "#aabbd0"; font.pixelSize: 11 }
                Item { Layout.preferredHeight: 28 }
                Repeater {
                    model: [{name: "Živý náhled", symbol: "◉"}, {name: "Nastavení", symbol: "⚙"}, {name: "Průvodce gesty", symbol: "✧"}]
                    delegate: Button {
                        id: nav
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true; implicitHeight: 44
                        hoverEnabled: true
                        onClicked: window.page = index
                        Accessible.name: modelData.name
                        background: Rectangle {
                            radius: 11
                            color: window.page === nav.index ? "#24c2dfff" : nav.hovered ? "#10ffffff" : "transparent"
                            border.width: nav.activeFocus ? 1 : 0; border.color: "#b4deff"
                        }
                        contentItem: Row {
                            spacing: 12; leftPadding: 12
                            Text { text: nav.modelData.symbol; color: window.page === nav.index ? "#cae6ff" : "#aabbd0"; font.pixelSize: 18; anchors.verticalCenter: parent.verticalCenter }
                            Text { text: nav.modelData.name; color: window.page === nav.index ? "#f0f6ff" : "#b4c2d3"; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Rectangle { Layout.fillWidth: true; height: 1; color: "#18ffffff" }
                Text { text: hyprlandSession ? "●  Hyprland / Wayland" : "●  Linux"; color: "#b4d9cc"; font.pixelSize: 11; Layout.topMargin: 8 }
                Text { text: "Zpracováno na tvém zařízení.\nObraz se nikam neposílá."; color: "#92a3b9"; font.pixelSize: 10; lineHeight: 1.5 }
                Text { text: "HANDMOUSE  /  0.4"; color: "#74859d"; font.pixelSize: 9; font.letterSpacing: 1.8; Layout.topMargin: 12 }
            }
        }
        ColumnLayout {
            Layout.fillWidth: true; Layout.fillHeight: true; spacing: 18
            RowLayout {
                Layout.fillWidth: true; Layout.topMargin: 12; spacing: 16
                ColumnLayout {
                    Layout.fillWidth: true; spacing: 7
                    Text { text: ["Přirozeně v pohybu.", "Přesně podle tebe.", "Malá gesta. Velké možnosti."][window.page]; color: "#f3f7ff"; font.pixelSize: 28; font.weight: Font.DemiBold }
                    Text { text: ["Ovládej plochu jednou rukou. Vlastním tempem.", "Nastavení se ukládá automaticky.", "Šest gest pro každodenní ovládání plochy."][window.page]; color: "#aebed2"; font.pixelSize: 13 }
                }
                Rectangle {
                    implicitWidth: badge.implicitWidth + 24; implicitHeight: 30; radius: 15
                    color: controller.running && !controller.paused ? "#203ecf9b" : "#16ffffff"
                    border.width: 1; border.color: "#22ffffff"
                    Text { id: badge; anchors.centerIn: parent; text: controller.busy ? "Připravuji…" : controller.paused ? "Pozastaveno" : controller.running ? "●  Aktivní" : "Připraveno"; color: controller.running && !controller.paused ? "#9ce4c8" : "#c3cedd"; font.pixelSize: 11 }
                }
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: errorRow.implicitHeight + 24
                visible: controller.error.length > 0
                color: "#30b77947"; radius: 12; border.color: "#60e7b386"
                RowLayout {
                    id: errorRow
                    anchors.fill: parent; anchors.margins: 12
                    Text { Layout.fillWidth: true; text: controller.error; color: "#ffe0be"; font.pixelSize: 12; wrapMode: Text.Wrap; textFormat: Text.PlainText }
                    ActionButton { text: "Zavřít"; compact: true; onClicked: controller.clearError() }
                }
            }
            StackLayout {
                currentIndex: window.page
                Layout.fillWidth: true; Layout.fillHeight: true
                ColumnLayout {
                    spacing: 14
                    GlassPanel {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 270
                        tintOpacity: 0.04; clip: true
                        Rectangle { anchors.fill: parent; anchors.margins: 1; radius: 21; color: "#650b1522" }
                        VideoItem { anchors.fill: parent; anchors.margins: 12; controller: window.controller; visible: controller.running }
                        ColumnLayout {
                            anchors.centerIn: parent; spacing: 14; visible: !controller.running
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter; width: 76; height: 76; radius: 26; color: "#12d5eaff"; border.color: "#24d5eaff"
                                Text { anchors.centerIn: parent; text: "◉"; color: "#b5d3f2"; font.pixelSize: 38 }
                            }
                            Text { Layout.alignment: Qt.AlignHCenter; text: controller.busy ? "Připravuji snímání" : "Prostor pro tvou ruku"; color: "#e4edf9"; font.pixelSize: 21; font.weight: Font.Medium }
                            Text { Layout.alignment: Qt.AlignHCenter; text: "Postav kameru před sebe a nech ruku v záběru.\nZačni náhledem a vyzkoušej jednotlivá gesta."; horizontalAlignment: Text.AlignHCenter; lineHeight: 1.5; color: "#9dadc2"; font.pixelSize: 12 }
                        }
                        Rectangle {
                            anchors.fill: parent; anchors.margins: 1; radius: 21; visible: controller.running && controller.paused; color: "#880a1423"
                            Column {
                                anchors.centerIn: parent; spacing: 10
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Pozastaveno"; color: "#f1f6ff"; font.pixelSize: 28; font.weight: Font.Medium }
                                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Prostředníček nebo tlačítko Pokračovat"; color: "#c0d0e2"; font.pixelSize: 12 }
                            }
                        }
                        Rectangle {
                            anchors.top: parent.top; anchors.left: parent.left; anchors.margins: 18
                            width: previewLabel.implicitWidth + 24; height: 28; radius: 14; color: "#a51a2639"; border.color: "#25ffffff"
                            Text { id: previewLabel; anchors.centerIn: parent; text: window.preferences.previewOnly ? "POUZE NÁHLED" : "OVLÁDÁNÍ KURZORU"; color: "#d9e7f7"; font.pixelSize: 9; font.letterSpacing: 1.2 }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom; anchors.horizontalCenter: parent.horizontalCenter; anchors.bottomMargin: 18
                            width: statusLabel.implicitWidth + 32; height: 34; radius: 17; color: "#ca172333"; visible: controller.running
                            Text { id: statusLabel; anchors.centerIn: parent; text: controller.status; color: "#e2f0ff"; font.pixelSize: 12 }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true; spacing: 12
                        Repeater {
                            model: [
                                {label: "KAMERA", value: controller.running ? controller.cameraFps.toFixed(0) + " fps" : "—"},
                                {label: "ROZPOZNÁVÁNÍ / NÁHLED", value: controller.running ? controller.fps.toFixed(0) + " fps" : "—"},
                                {label: "INFERENCE · CPU", value: controller.running ? controller.inferenceMs.toFixed(0) + " ms" : "—"},
                                {label: "RUKA V ZÁBĚRU", value: controller.handVisible ? (controller.confidence * 100).toFixed(0) + " %" : "—"}
                            ]
                            delegate: GlassPanel {
                                required property var modelData
                                Layout.fillWidth: true; implicitHeight: 70; radius: 14; tintOpacity: 0.045
                                Column { anchors.left: parent.left; anchors.leftMargin: 16; anchors.verticalCenter: parent.verticalCenter; spacing: 7
                                    Text { text: modelData.label; color: "#9badc3"; font.pixelSize: 9; font.letterSpacing: 1.2 }
                                    Text { text: modelData.value; color: "#e4eefb"; font.pixelSize: 18; font.weight: Font.Medium }
                                }
                            }
                        }
                    }
                    GlassPanel {
                        Layout.fillWidth: true; implicitHeight: 77; radius: 16; tintOpacity: 0.06
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 15; spacing: 10
                            ActionButton { text: controller.running ? "Zastavit" : "Spustit kameru"; primary: !controller.running; enabled: !controller.busy; onClicked: controller.running ? controller.stop() : controller.start() }
                            ActionButton { text: controller.paused ? "Pokračovat" : "Pozastavit"; enabled: controller.running; onClicked: controller.togglePause() }
                            Item { Layout.fillWidth: true }
                            SettingToggle { title: "Ovládat kurzor"; description: ""; checked: !window.preferences.previewOnly; onToggled: function(value) { settings.setValue("previewOnly", !value) } }
                        }
                    }
                    Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: controller.running; text: controller.cameraMode + " · odezva " + controller.latencyMs.toFixed(0) + " ms · kreslení " + controller.renderMs.toFixed(1) + " ms"; color: "#9badc3"; font.pixelSize: 10 }
                    Text { text: "Esc zastaví kameru  ·  Mezerník přepne pauzu  ·  Globální zkratky najdeš v průvodci"; color: "#93a5bd"; font.pixelSize: 10; Layout.alignment: Qt.AlignHCenter; Layout.bottomMargin: 6 }
                }
                ScrollView {
                    id: settingsScroll
                    clip: true
                    contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ColumnLayout {
                        width: settingsScroll.availableWidth; spacing: 16
                        GlassPanel {
                            Layout.fillWidth: true; implicitHeight: cursorSettings.implicitHeight + 40
                            ColumnLayout {
                                id: cursorSettings
                                anchors.fill: parent; anchors.margins: 20; spacing: 18
                                Text { text: "Kurzor a scrollování"; color: "#f0f5fd"; font.pixelSize: 17; font.weight: Font.DemiBold }
                                SettingSlider { Layout.fillWidth: true; title: "Rychlost kurzoru"; description: "Kolik pohybu na ploše udělá malý pohyb ruky."; from: 0.2; to: 3; value: window.preferences.speed; displayValue: value.toFixed(1) + "×"; onEdited: function(v) { settings.setValue("speed", v) } }
                                SettingSlider { Layout.fillWidth: true; title: "Vyhlazování pohybu"; description: "Méně = rychlá odezva. Více = klidnější kurzor."; value: window.preferences.smoothing; displayValue: Math.round(value * 100) + " %"; onEdited: function(v) { settings.setValue("smoothing", v) } }
                                SettingSlider { Layout.fillWidth: true; title: "Rychlost scrollování"; description: "Sevři pěst a posuň ji nahoru nebo dolů od výchozí polohy."; from: 2; to: 30; stepSize: 1; value: window.preferences.scrollSpeed; displayValue: value.toFixed(0); onEdited: function(v) { settings.setValue("scrollSpeed", v) } }
                            }
                        }
                        GlassPanel {
                            Layout.fillWidth: true; implicitHeight: cameraSettings.implicitHeight + 40
                            ColumnLayout {
                                id: cameraSettings; anchors.fill: parent; anchors.margins: 20; spacing: 18
                                Text { text: "Kamera a rozpoznávání"; color: "#f0f5fd"; font.pixelSize: 17; font.weight: Font.DemiBold }
                                RowLayout {
                                    Layout.fillWidth: true
                                    ComboBox {
                                        id: cameraPicker
                                        Layout.fillWidth: true
                                        model: controller.cameras; textRole: "label"; valueRole: "index"
                                        currentIndex: {
                                            const choices = controller.cameras
                                            for (let i = 0; i < choices.length; ++i)
                                                if (choices[i].index === window.preferences.camera) return i
                                            return -1
                                        }
                                        displayText: count === 0 ? "Žádná kamera — připoj ji a obnov seznam" : currentIndex < 0 ? "Vyber kameru" : currentText
                                        onActivated: settings.setValue("camera", currentValue)
                                        Accessible.name: "Kamera"
                                        palette.buttonText: "#edf3fb"; palette.button: "#344457"; palette.text: "#edf3fb"; palette.base: "#243346"; palette.highlight: "#466382"
                                    }
                                    ActionButton { text: "Obnovit"; compact: true; onClicked: controller.refreshCameras() }
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: controller.fpsOptions; textRole: "label"; valueRole: "value"
                                    currentIndex: window.preferences.cameraFps === 60 ? 1 : 0
                                    onActivated: {
                                        if (controller.fpsOptions[currentIndex].available) settings.setValue("cameraFps", currentValue)
                                        else currentIndex = Qt.binding(function() { return window.preferences.cameraFps === 60 ? 1 : 0 })
                                    }
                                    delegate: ItemDelegate {
                                        required property var modelData
                                        width: ListView.view.width
                                        text: modelData.label; enabled: modelData.available
                                    }
                                    Accessible.name: "Požadovaná snímková frekvence"
                                    palette.buttonText: "#edf3fb"; palette.button: "#344457"; palette.text: "#edf3fb"; palette.base: "#243346"; palette.highlight: "#466382"
                                }
                                ComboBox {
                                    Layout.fillWidth: true
                                    model: controller.resolutionOptions; textRole: "label"; valueRole: "value"
                                    currentIndex: {
                                        const choices = controller.resolutionOptions
                                        for (let i = 0; i < choices.length; ++i)
                                            if (choices[i].value === window.preferences.cameraResolution) return i
                                        return 0
                                    }
                                    onActivated: settings.setValue("cameraResolution", currentValue)
                                    Accessible.name: "Rozlišení kamery"
                                    palette.buttonText: "#edf3fb"; palette.button: "#344457"; palette.text: "#edf3fb"; palette.base: "#243346"; palette.highlight: "#466382"
                                }
                                Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Změna kamery, rozlišení nebo FPS snímání automaticky restartuje. Za šera může skutečná rychlost klesnout; pomůže více světla."; color: "#9badc3"; font.pixelSize: 11 }
                                Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; visible: controller.cameraMode.length > 0; text: controller.cameraMode; color: "#b9cee5"; font.pixelSize: 11 }
                                Text { text: "Výpočet: " + controller.backend; color: "#9badc3"; font.pixelSize: 11 }
                                Text { Layout.fillWidth: true; wrapMode: Text.WordWrap; text: "Ochrana pohybu: při výpadku nebo nejistém scrollování se ovládání zastaví. Obnovíš ho stabilní otevřenou dlaní (0,2 s)."; color: "#b9cee5"; font.pixelSize: 11 }
                                SettingToggle { Layout.fillWidth: true; title: "Zrcadlit obraz"; description: "Ruka se pohybuje stejným směrem jako kurzor."; checked: window.preferences.mirror; onToggled: function(v) { settings.setValue("mirror", v) } }
                                SettingToggle { Layout.fillWidth: true; title: "Zobrazit body ruky"; description: "Pomocná kresba pro kontrolu rozpoznávání."; checked: window.preferences.skeleton; onToggled: function(v) { settings.setValue("skeleton", v) } }
                                SettingSlider { Layout.fillWidth: true; title: "Citlivost dotyku prstů"; description: "Vyšší hodnota dovolí větší mezeru mezi prsty."; from: 0.15; to: 0.6; value: window.preferences.pinchThreshold; displayValue: value.toFixed(2); onEdited: function(v) { settings.setValue("pinchThreshold", v) } }
                                SettingSlider { Layout.fillWidth: true; title: "Jistota rozpoznání"; description: "Vyšší hodnota odmítne nejisté detekce, ale potřebuje lepší světlo."; from: 0.5; to: 0.9; value: window.preferences.confidence; displayValue: Math.round(value * 100) + " %"; onEdited: function(v) { settings.setValue("confidence", v) } }
                            }
                        }
                        GlassPanel {
                            Layout.fillWidth: true; implicitHeight: appearanceSettings.implicitHeight + 40
                            ColumnLayout {
                                id: appearanceSettings; anchors.fill: parent; anchors.margins: 20; spacing: 16
                                Text { text: "Vzhled"; color: "#f0f5fd"; font.pixelSize: 17; font.weight: Font.DemiBold }
                                SettingSlider { Layout.fillWidth: true; title: "Krytí skla"; description: "Skutečné rozostření pozadí zajišťuje Hyprland. Při horší čitelnosti zvyš krytí."; from: 0.6; to: 1; value: window.preferences.glassOpacity; displayValue: Math.round(value * 100) + " %"; onEdited: function(v) { settings.setValue("glassOpacity", v) } }
                            }
                        }
                        ActionButton { text: "Obnovit výchozí nastavení"; onClicked: resetDialog.open(); Layout.bottomMargin: 16 }
                    }
                }
                ScrollView {
                    id: guideScroll
                    clip: true; contentWidth: availableWidth
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                    ColumnLayout {
                        width: guideScroll.availableWidth; spacing: 12
                        Repeater {
                            model: [
                                {number: "01", title: "Otevřená ruka", detail: "Roztáhni prsty a pohybuj rukou. Kurzor sleduje pohyb dlaně. Při změně gesta nepřeskočí."},
                                {number: "02", title: "Pěst → scrollování", detail: "Sevřením pěsti vznikne kotva. Nad ní scrolluješ nahoru, pod ní dolů. Čím dál od kotvy, tím rychleji."},
                                {number: "03", title: "Palec + ukazováček → klik / tažení", detail: "Krátce spoj prsty pro kliknutí. Drž je spojené a pohybuj rukou pro přetahování."},
                                {number: "04", title: "Palec + malíček → Super + tažení", detail: "Držením dotyku přesouvej okna v Hyprlandu. Vyžaduje vazbu SUPER + levé tlačítko na přesouvání oken."},
                                {number: "05", title: "Palec + prsteníček → kolečko", detail: "Spoj prsty pro jedno kliknutí prostředním tlačítkem. Před dalším kliknutím je odděl."},
                                {number: "06", title: "Samotný prostředníček → pauza", detail: "Ostatní prsty schovej a gesto podrž půl sekundy. Stejným gestem ovládání opět zapneš."}
                            ]
                            delegate: GlassPanel {
                                required property var modelData
                                Layout.fillWidth: true; implicitHeight: gestureRow.implicitHeight + 30; radius: 16; tintOpacity: 0.055
                                RowLayout {
                                    id: gestureRow; anchors.fill: parent; anchors.margins: 15; spacing: 16
                                    Text { text: modelData.number; color: "#95bbdd"; font.pixelSize: 21; font.weight: Font.Light; Layout.preferredWidth: 32 }
                                    ColumnLayout {
                                        Layout.fillWidth: true; spacing: 7
                                        Text { text: modelData.title; color: "#edf4ff"; font.pixelSize: 14; font.weight: Font.Medium; wrapMode: Text.Wrap; Layout.fillWidth: true }
                                        Text { text: modelData.detail; color: "#a9bbd0"; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true; lineHeight: 1.3 }
                                    }
                                }
                            }
                        }
                        Text { Layout.fillWidth: true; text: "Začni v režimu Pouze náhled. Gesta drž čelem ke kameře; při dotyku dvou prstů nech ostatní volné. Při ztrátě ruky se myš i Super automaticky uvolní."; color: "#aebed2"; font.pixelSize: 12; wrapMode: Text.Wrap; lineHeight: 1.5 }
                        GlassPanel {
                            Layout.fillWidth: true; implicitHeight: shortcuts.implicitHeight + 32; radius: 16
                            ColumnLayout {
                                id: shortcuts; anchors.fill: parent; anchors.margins: 16; spacing: 8
                                Text { text: "Zkratky i mimo okno aplikace"; color: "#edf4ff"; font.pixelSize: 14 }
                                Text { Layout.fillWidth: true; text: "V docs/hyprland.lua je připravené Super + Shift + F9 pro pauzu a Super + Shift + F10 pro zastavení. Nejprve je přidej do konfigurace Hyprlandu podle README."; color: "#a9bbd0"; font.pixelSize: 12; wrapMode: Text.Wrap; lineHeight: 1.4 }
                            }
                        }
                        Item { height: 12 }
                    }
                }
            }
        }
    }
    Dialog {
        id: resetDialog
        anchors.centerIn: parent
        title: "Obnovit nastavení?"
        modal: true
        standardButtons: Dialog.Reset | Dialog.Cancel
        onReset: { settings.reset(); close() }
        palette.window: "#233145"; palette.windowText: "#edf3fb"; palette.text: "#edf3fb"; palette.button: "#34465c"; palette.buttonText: "#edf3fb"
        Label { text: "Vrátí rychlost, gesta, kameru a vzhled na výchozí hodnoty."; color: "#c1d0e0" }
    }
}
