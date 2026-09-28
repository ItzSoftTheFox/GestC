#include "app/controller.hpp"
#include "ui/video_item.hpp"
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QCommandLineParser>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSurfaceFormat>
#include <iostream>

int main(int argc, char* argv[]) {
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QSurfaceFormat format; format.setAlphaBufferSize(8); QSurfaceFormat::setDefaultFormat(format);
    QGuiApplication app(argc, argv);
    app.setOrganizationName(QStringLiteral("HandMouse"));
    app.setApplicationName(QStringLiteral("HandMouse"));
    app.setApplicationVersion(QStringLiteral("0.4.0"));
    app.setDesktopFileName(QStringLiteral("handmouse"));
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Lokální ovládání myši gesty pro Linux / Hyprland."));
    parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"model-dir", "Adresář s modely TFLite.", "directory"});
    parser.addOption({"status", "Vypsat stav běžící instance jako JSON."});
    parser.addOption({"stop", "Zastavit běžící instanci (globální zkratka)."});
    parser.addOption({"toggle-pause", "Přepnout pauzu běžící instance."});
    parser.addOption({"smoke-test", "Ověřit načtení UI bez kamery a skončit."});
    parser.addOption({"screenshot", "Uložit snímek UI a skončit, bez kamery.", "file"});
    parser.addOption({"page", "Stránka snímku: live, settings, gestures.", "page", "live"});
    parser.process(app);
    const bool testing = parser.isSet("smoke-test") || parser.isSet("screenshot");
    const QString runtime = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString socketName = runtime + QStringLiteral("/handmouse.sock");
    QLocalServer server;
    QLockFile lock(runtime + QStringLiteral("/handmouse.lock"));
    if (!testing) {
        QLocalSocket client;
        client.connectToServer(socketName);
        if (client.waitForConnected(250)) {
            const QByteArray command = parser.isSet("stop") ? "stop\n" : parser.isSet("toggle-pause") ? "pause\n" : parser.isSet("status") ? "status\n" : "show\n";
            client.write(command); client.flush();
            if (!client.canReadLine() && !client.waitForReadyRead(2000)) {
                std::cerr << "Běžící instance neodpovídá.\n"; return 1;
            }
            const auto reply = client.readAll();
            if (parser.isSet("status")) std::cout << reply.constData();
            client.disconnectFromServer();
            return 0;
        }
        if (parser.isSet("stop") || parser.isSet("toggle-pause") || parser.isSet("status")) {
            std::cerr << "HandMouse neběží.\n"; return 1;
        }
        if (!lock.tryLock()) { std::cerr << "Jiná instance HandMouse právě startuje.\n"; return 1; }
        QLocalServer::removeServer(socketName);
        server.setSocketOptions(QLocalServer::UserAccessOption);
        if (!server.listen(socketName)) { std::cerr << server.errorString().toStdString() << '\n'; return 1; }
    }
    QString modelDirectory = parser.value("model-dir");
    if (modelDirectory.isEmpty()) {
        const QStringList candidates{QCoreApplication::applicationDirPath() + "/../share/handmouse/models",
                                     QStringLiteral(HANDMOUSE_INSTALL_MODELS), QStringLiteral(HANDMOUSE_SOURCE_MODELS)};
        for (const auto& candidate : candidates)
            if (QFileInfo::exists(candidate + "/hand_detector.tflite")) { modelDirectory = candidate; break; }
    }
    AppSettings settings;
    Controller controller(&settings, modelDirectory);
    qmlRegisterType<VideoItem>("HandMouse", 1, 0, "VideoItem");
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appController", &controller);
    engine.rootContext()->setContextProperty("appSettings", &settings);
    engine.rootContext()->setContextProperty("hyprlandSession", !qEnvironmentVariableIsEmpty("HYPRLAND_INSTANCE_SIGNATURE"));
    bool qmlWarnings = false;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [&](const QList<QQmlError>&) { qmlWarnings = true; });
    engine.load(QUrl(QStringLiteral("qrc:/src/ui/main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, &Controller::stop);
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (auto* socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            const auto read = [socket, &controller, window] {
                if (!socket->canReadLine()) return;
                const QByteArray command = socket->readLine(32).trimmed();
                if (command == "stop") controller.emergencyStop();
                else if (command == "pause") controller.togglePause();
                else if (command == "show") { window->showNormal(); window->raise(); window->requestActivate(); }
                if (command == "status") {
                    const QJsonObject state{{"running", controller.running()}, {"paused", controller.paused()},
                        {"busy", controller.busy()}, {"inputReady", controller.inputReady()}, {"status", controller.status()}};
                    socket->write(QJsonDocument(state).toJson(QJsonDocument::Compact) + '\n');
                } else socket->write("ok\n");
                socket->flush(); socket->disconnectFromServer();
            };
            QObject::connect(socket, &QLocalSocket::readyRead, socket, read);
            read();
        }
    });
    if (testing) {
        const QString page = parser.value("page");
        window->setProperty("page", page == "settings" ? 1 : page == "gestures" ? 2 : 0);
        QTimer::singleShot(800, &app, [&] {
            bool ok = !qmlWarnings;
            if (parser.isSet("screenshot")) ok = window->grabWindow().save(parser.value("screenshot")) && ok;
            app.exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
