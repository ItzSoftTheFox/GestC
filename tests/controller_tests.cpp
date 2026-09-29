#include "app/controller.hpp"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <stdexcept>

// Simulate only camera delivery; exercise the real controller/restart/watchdog code.
// No capture or uinput is started. Test settings are always preview-only.
std::atomic<int> opens{0};
std::atomic<bool> stale{false};
CameraWorker::~CameraWorker() { requestInterruption(); wait(); }
quint64 CameraWorker::configure(const handmouse::Settings& settings) {
    std::lock_guard<std::mutex> lock(mutex_); settings_ = settings; return ++revision_;
}
double CameraWorker::now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void CameraWorker::run() {
    framePending_ = false;
    msleep(20);
    handmouse::Settings initial;
    { std::lock_guard<std::mutex> lock(mutex_); initial = settings_; }
    if (isInterruptionRequested()) return;
    if (initial.camera == 63) { emit failed(QStringLiteral("Disconnected camera")); return; }
    ++opens;
    emit modeReady(QStringLiteral("simulated 30 fps"), 30);
    emit cameraOpened();
    while (!isInterruptionRequested()) {
        if (!framePending_.exchange(true)) {
            TrackingFrame frame;
            { std::lock_guard<std::mutex> lock(mutex_); frame.revision = revision_; }
            frame.timestamp = now() - (stale ? 0.5 : 0);
            frame.cameraFps = 30;
            frame.hand = handmouse::Hand{}; frame.hand->confidence = .95;
            emit frameReady(frame);
        }
        msleep(10);
    }
}
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool until(const std::function<bool()>& condition, int timeout = 2000) {
    QElapsedTimer timer; timer.start();
    do { QCoreApplication::processEvents(); if (condition()) return true; QThread::msleep(1); } while (timer.elapsed() < timeout);
    return false;
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, directory.path());
    app.setOrganizationName("HandMouseTests"); app.setApplicationName("Controller");
    try {
        AppSettings settings; Controller controller(&settings, {});
        controller.start(); require(until([&] { return controller.running(); }), "start camera");
        settings.setValue("cameraFps", 60);
        require(until([&] { return opens == 2 && controller.running(); }), "FPS change restarts camera");
        require(controller.negotiatedFps() == 30, "report actual negotiated rate instead of requested 60");
        settings.setValue("cameraFps", 30); settings.setValue("cameraFps", 60); settings.setValue("cameraFps", 30);
        require(until([&] { return opens >= 3 && controller.running(); }), "rapid setting changes restart with latest settings");
        const int beforeResolution = opens;
        settings.setValue("cameraResolution", "1280x720");
        require(until([&] { return opens > beforeResolution && controller.running(); }), "resolution change restarts camera");
        const int count = opens;
        settings.setValue("cameraFps", 60); controller.emergencyStop();
        require(until([&] { return !controller.busy(); }), "emergency stop finishes");
        until([] { return false; }, 100);
        require(!controller.running() && opens == count, "emergency stop cancels pending restart");
        controller.start(); require(until([&] { return controller.handVisible(); }), "new start accepts fresh frames");
        stale = true; require(until([&] { return !controller.handVisible(); }), "stale frames clear hand state");
        stale = false;
        settings.setValue("camera", 63);
        require(until([&] { return !controller.busy() && !controller.error().isEmpty(); }), "disconnect during restart reports error");
        require(!controller.running() && !controller.inputReady(), "failure closes input and does not loop restart");
        settings.setValue("camera", 0); controller.start();
        require(until([&] { return controller.running(); }), "recover after reconnect");
        controller.stop(); require(until([&] { return !controller.busy(); }), "final stop");
        std::cout << "Controller restart, cancellation, stale frames and reconnect passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
