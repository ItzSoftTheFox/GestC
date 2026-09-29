#include "controller.hpp"
#include <QDir>
#include <QFile>
#include "tracking/camera_device.hpp"
#include <cmath>
#include <set>

Controller::Controller(AppSettings* settings, QString modelDirectory, QObject* parent)
    : QObject(parent), settings_(settings), previousSettings_(settings->snapshot()) {
    qRegisterMetaType<TrackingFrame>();
    worker_.setModelDirectory(std::move(modelDirectory));
    connect(settings_, &AppSettings::changed, this, &Controller::settingsChanged);
    connect(&worker_, &CameraWorker::frameReady, this, &Controller::receive);
    connect(&worker_, &CameraWorker::modeReady, this, [this](const QString& description, double fps) {
        if (stopping_) return;
        cameraMode_ = description; negotiatedFps_ = fps; emit changed();
    });
    connect(&worker_, &CameraWorker::cameraOpened, this, [this] {
        if (stopping_) return;
        running_ = true; busy_ = false;
        status_ = QStringLiteral("Ukaž otevřenou ruku");
        lastFrame_ = CameraWorker::now(); watchdog_.start(); emit changed();
    });
    connect(&worker_, &CameraWorker::failed, this, [this](const QString& error) {
        error_ = error; input_.close(); engine_.reset(); emit changed();
    });
    connect(&worker_, &QThread::finished, this, [this] {
        watchdog_.stop(); input_.close(); engine_.reset();
        running_ = busy_ = stopping_ = handVisible_ = false;
        fps_ = confidence_ = inferenceMs_ = cameraFps_ = negotiatedFps_ = latencyMs_ = renderMs_ = 0;
        cameraMode_.clear();
        status_ = QStringLiteral("Zastaveno");
        emit frameReady(QImage()); emit changed();
        if (restartRequested_) QTimer::singleShot(0, this, [this] {
            if (restartRequested_) { restartRequested_ = false; start(); }
        });
    });
    watchdog_.setInterval(100);
    connect(&watchdog_, &QTimer::timeout, this, [this] {
        if (running_ && CameraWorker::now() - lastFrame_ > 0.35) {
            input_.release(); engine_.suspend(); handVisible_ = false;
            confidence_ = fps_ = cameraFps_ = 0; status_ = QStringLiteral("Čekám na obraz kamery…"); emit changed();
        }
    });
    refreshCameras();
}
Controller::~Controller() {
    input_.close(); worker_.requestInterruption(); worker_.wait();
}
void Controller::start() {
    if (worker_.isRunning() || busy_) return;
    error_.clear(); engine_.setPaused(false); lastFrame_ = 0;
    fpsWindow_ = 0; fpsFrames_ = 0; cameraMode_.clear();
    if (!settings_->snapshot().previewOnly && !input_.open()) {
        error_ = input_.error(); emit changed(); return;
    }
    revision_ = worker_.configure(settings_->snapshot());
    busy_ = true; stopping_ = false; status_ = QStringLiteral("Načítám modely a kameru…");
    emit changed(); worker_.start();
}
void Controller::stop() {
    restartRequested_ = false;
    input_.close(); engine_.reset(); watchdog_.stop();
    running_ = false; handVisible_ = false;
    if (worker_.isRunning()) {
        stopping_ = true; busy_ = true; status_ = QStringLiteral("Ukončuji snímání…");
        worker_.requestInterruption();
    } else { busy_ = false; status_ = QStringLiteral("Zastaveno"); }
    emit changed();
}
void Controller::emergencyStop() { stop(); }
void Controller::togglePause() {
    input_.release(); engine_.setPaused(!engine_.paused());
    status_ = engine_.paused() ? QStringLiteral("Pozastaveno") : QStringLiteral("Ukaž otevřenou ruku");
    emit changed();
}
void Controller::receive(const TrackingFrame& frame) {
    worker_.acknowledge();
    if (!running_ || stopping_ || frame.revision != revision_) return;
    const double now = CameraWorker::now();
    if (now - frame.timestamp > 0.25) {
        input_.release(); engine_.suspend();
        handVisible_ = false; confidence_ = 0;
        status_ = QStringLiteral("Snímání je příliš pomalé — vstup pozastaven");
        emit frameReady(frame.image); emit changed(); return;
    }
    if (fpsWindow_ == 0) { fpsWindow_ = now; fpsFrames_ = 0; }
    else {
        ++fpsFrames_;
        if (now - fpsWindow_ >= 1) {
            fps_ = fpsFrames_ / (now - fpsWindow_); fpsWindow_ = now; fpsFrames_ = 0;
        }
    }
    cameraFps_ = frame.cameraFps;
    latencyMs_ = (now - frame.timestamp) * 1000;
    lastFrame_ = now;
    handVisible_ = frame.hand.has_value();
    confidence_ = frame.hand ? frame.hand->confidence : 0;
    inferenceMs_ = frame.inferenceMs;
    const auto command = engine_.update(frame.hand, frame.timestamp, settings_->snapshot());
    status_ = engine_.paused() ? QStringLiteral("Pozastaveno")
        : command.safetyBlocked ? QStringLiteral("Ochrana pohybu — ukaž stabilně otevřenou dlaň")
        : !handVisible_ ? QStringLiteral("Hledám ruku…") : QString::fromUtf8(handmouse::GestureEngine::poseName(command.pose));
    if (!settings_->snapshot().previewOnly && input_.ready() && !input_.apply(command)) {
        error_ = input_.error(); stop();
    }
    emit frameReady(frame.image); emit changed();
}
void Controller::settingsChanged() {
    const auto settings = settings_->snapshot();
    input_.release(); engine_.reset();
    const bool modeChanged = settings.camera != previousSettings_.camera || settings.cameraFps != previousSettings_.cameraFps
        || settings.cameraWidth != previousSettings_.cameraWidth || settings.cameraHeight != previousSettings_.cameraHeight;
    if (modeChanged && ((!stopping_ && (worker_.isRunning() || busy_)) || restartRequested_)) {
        stop(); restartRequested_ = true;
    }
    if (settings.previewOnly) input_.close();
    else if (running_ && !input_.open()) {
        error_ = input_.error();
        stop();
    }
    previousSettings_ = settings;
    revision_ = worker_.configure(settings);
    if (modeChanged) refreshModes();
    emit changed();
}
void Controller::refreshCameras() {
    cameras_.clear();
    const QDir devices(QStringLiteral("/sys/class/video4linux"));
    for (const auto& entry : devices.entryList({QStringLiteral("video*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        bool ok = false;
        const int index = entry.mid(5).toInt(&ok);
        if (!ok) continue;
        QFile name(devices.filePath(entry + QStringLiteral("/name")));
        QString label = entry;
        if (name.open(QIODevice::ReadOnly)) label = QString::fromUtf8(name.readAll()).trimmed() + QStringLiteral(" · ") + entry;
        const auto capabilities = cameraCapabilities(index);
        if (!capabilities.capture && capabilities.error.empty()) continue; // Metadata-only node.
        cameras_.append(QVariantMap{{"label", label}, {"index", index}});
    }
    refreshModes();
}
void Controller::refreshModes() {
    fpsOptions_.clear();
    const auto settings = settings_->snapshot();
    const auto capabilities = cameraCapabilities(settings.camera);
    for (const int fps : {30, 60}) {
        const auto mode = chooseCameraMode(capabilities.modes, fps, settings.cameraWidth, settings.cameraHeight);
        const bool known = !capabilities.modes.empty();
        const bool supported = mode && std::abs(mode->fps - fps) <= 0.5
            && (!settings.cameraWidth || (mode->width == settings.cameraWidth && mode->height == settings.cameraHeight));
        QString label = QStringLiteral("%1 fps").arg(fps);
        if (supported) label += QStringLiteral(" · %1 × %2").arg(mode->width).arg(mode->height);
        else label += known ? QStringLiteral(" · kamera nepodporuje") : QStringLiteral(" · ověří se při spuštění");
        fpsOptions_.append(QVariantMap{{"label", label}, {"value", fps}, {"available", supported || !known}});
    }
    resolutionOptions_.clear();
    resolutionOptions_.append(QVariantMap{{"label", QStringLiteral("Automaticky · přednost rychlosti")}, {"value", "auto"}});
    std::set<std::pair<int, int>> sizes;
    for (const auto& mode : capabilities.modes) sizes.emplace(mode.width, mode.height);
    if (sizes.empty()) { sizes.emplace(640, 480); sizes.emplace(1280, 720); }
    if (settings.cameraWidth) sizes.emplace(settings.cameraWidth, settings.cameraHeight);
    for (const auto& [width, height] : sizes) {
        const auto mode = chooseCameraMode(capabilities.modes, settings.cameraFps, width, height);
        QString label = QStringLiteral("%1 × %2").arg(width).arg(height);
        if (mode && mode->width == width && mode->height == height) label += QStringLiteral(" · %1 fps").arg(mode->fps, 0, 'f', 0);
        else label += capabilities.modes.empty() ? QStringLiteral(" · ověří se při spuštění") : QStringLiteral(" · nedostupné, použije se náhradní režim");
        resolutionOptions_.append(QVariantMap{{"label", label}, {"value", QStringLiteral("%1x%2").arg(width).arg(height)}});
    }
    emit camerasChanged();
}
