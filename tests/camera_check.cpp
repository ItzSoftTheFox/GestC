#include "tracking/camera_worker.hpp"
#include "gestures/gesture_engine.hpp"
#include <QCoreApplication>
#include <QTimer>
#include <iostream>

// Opt-in hardware check of the same worker used by the UI. No input, no image storage.
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) { std::cerr << "camera_check MODEL_DIR [CAMERA_INDEX] [30|60] [ACK_DELAY_MS] [WIDTHxHEIGHT]\n"; return 2; }
    qRegisterMetaType<TrackingFrame>();
    CameraWorker worker;
    handmouse::Settings settings;
    settings.camera = argc > 2 ? QString::fromLocal8Bit(argv[2]).toInt() : 0;
    settings.cameraFps = argc > 3 ? QString::fromLocal8Bit(argv[3]).toInt() : 30;
    const int ackDelay = argc > 4 ? QString::fromLocal8Bit(argv[4]).toInt() : 0;
    if ((settings.cameraFps != 30 && settings.cameraFps != 60) || ackDelay < 0 || ackDelay > 1000) return 2;
    if (argc > 5) {
        const auto dimensions = QString::fromLocal8Bit(argv[5]).split('x');
        if (dimensions.size() != 2) return 2;
        settings.cameraWidth = dimensions[0].toInt(); settings.cameraHeight = dimensions[1].toInt();
        if (settings.cameraWidth <= 0 || settings.cameraHeight <= 0) return 2;
    }
    const auto revision = worker.configure(settings);
    worker.setModelDirectory(QString::fromLocal8Bit(argv[1]));
    int frames = 0, hands = 0;
    double totalMs = 0, totalWait = 0, totalDecode = 0, totalPreview = 0, totalDelivery = 0;
    double first = 0, last = 0, cameraFps = 0, maxAge = 0;
    quint64 skipped = 0;
    bool failed = false;
    QObject::connect(&worker, &CameraWorker::frameReady, &app, [&](const TrackingFrame& frame) {
        if (ackDelay) QTimer::singleShot(ackDelay, &app, [&] { worker.acknowledge(); });
        else worker.acknowledge();
        if (frame.revision != revision) failed = true;
        ++frames; if (frame.hand) ++hands; totalMs += frame.inferenceMs;
        if (first == 0) first = CameraWorker::now();
        last = CameraWorker::now(); cameraFps = frame.cameraFps; skipped = frame.skippedFrames;
        const double age = (last - frame.timestamp) * 1000;
        maxAge = std::max(maxAge, age);
        totalWait += frame.waitMs; totalDecode += frame.decodeMs; totalPreview += frame.previewMs;
        totalDelivery += age - frame.decodeMs - frame.inferenceMs - frame.previewMs;
    });
    QObject::connect(&worker, &CameraWorker::modeReady, &app, [](const QString& description, double) {
        std::cout << description.toStdString() << '\n';
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
        if (frames) std::cout << "camera " << cameraFps << " fps; delivered " << (last > first ? (frames - 1) / (last - first) : 0)
            << " fps; wait " << totalWait / frames << " ms; decode " << totalDecode / frames
            << " ms; preview " << totalPreview / frames << " ms; other/delivery " << totalDelivery / frames
            << " ms; max age " << maxAge << " ms; skipped " << skipped << '\n';
        app.exit(frames && !failed ? 0 : 1);
    });
    QTimer::singleShot(12000, &app, [&] { failed = true; worker.requestInterruption(); });
    worker.start();
    return app.exec();
}
