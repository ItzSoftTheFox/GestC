#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "core/gesture_scanner.hpp"
#include "ui/video_item.hpp"

using namespace Qt::StringLiterals;

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    // Změníme název "HandMouse.Components" na jednoduché slovo "CustomElements"
    qmlRegisterType<VideoItem>("CustomElements", 1, 0, "VideoItem");

    GestureScanner scanner;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("scannerBackend", &scanner);

    const QUrl url(u"file:../src/ui/main.qml"_s);
    engine.load(url);

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}