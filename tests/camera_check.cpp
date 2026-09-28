#include "tracking/camera_worker.hpp"
#include "gestures/gesture_engine.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

// Opt-in hardware check of the same worker used by the UI. No input, no image storage.
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { std::cerr << "camera_check MODEL_DIR [CAMERA_INDEX]\n"; return 2; }
    qRegisterMetaType<TrackingFrame>();
    CameraWorker worker;
    handmouse::Settings settings;
    settings.camera = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 0;
    const auto revision = worker.configure(settings);
    worker.setModelDirectory(QString::fromLocal8Bit(argv[1]));
    int frames = 0, hands = 0;
    double totalMs = 0;
    bool failed = false;
    QObject::connect(&worker, &CameraWorker::frameReady, &app, [&](const TrackingFrame& frame) {
        worker.acknowledge();
        if (frame.revision != revision) failed = true;
        ++frames; if (frame.hand) ++hands; totalMs += frame.inferenceMs;
    });
    QObject::connect(&worker, &CameraWorker::cameraOpened, &app, [&] {
        QTimer::singleShot(5000, &app, [&] { worker.requestInterruption(); });
    });
    QObject::connect(&worker, &CameraWorker::failed, &app, [&](const QString& message) {
        std::cerr << message.toStdString() << '\n'; failed = true;
    });
    QObject::connect(&worker, &QThread::finished, &app, [&] {
        std::cout << frames << " worker frames; " << hands << " with hand; mean inference "
                  << (frames ? totalMs / frames : 0) << " ms; clean worker shutdown\n";
        app.exit(frames && !failed ? 0 : 1);
    });
    QTimer::singleShot(12000, &app, [&] { failed = true; worker.requestInterruption(); });
    worker.start();
    return app.exec();
}
