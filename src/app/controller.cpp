#include "controller.hpp"
#include <QDir>
#include <QFile>

Controller::Controller(AppSettings* settings, QString modelDirectory, QObject* parent)
    : QObject(parent), settings_(settings), previousSettings_(settings->snapshot()) {
    qRegisterMetaType<TrackingFrame>();
    worker_.setModelDirectory(std::move(modelDirectory));
    connect(settings_, &AppSettings::changed, this, &Controller::settingsChanged);
    connect(&worker_, &CameraWorker::frameReady, this, &Controller::receive);
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
        fps_ = confidence_ = inferenceMs_ = 0;
        status_ = QStringLiteral("Zastaveno");
        emit frameReady(QImage()); emit changed();
    });
    watchdog_.setInterval(100);
    connect(&watchdog_, &QTimer::timeout, this, [this] {
        if (running_ && CameraWorker::now() - lastFrame_ > 0.35) {
            input_.release(); engine_.reset(); handVisible_ = false;
            confidence_ = fps_ = 0; status_ = QStringLiteral("Čekám na obraz kamery…"); emit changed();
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
    if (!settings_->snapshot().previewOnly && !input_.open()) {
        error_ = input_.error(); emit changed(); return;
    }
    revision_ = worker_.configure(settings_->snapshot());
    busy_ = true; stopping_ = false; status_ = QStringLiteral("Načítám modely a kameru…");
    emit changed(); worker_.start();
}
void Controller::stop() {
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
        input_.release(); engine_.reset();
        handVisible_ = false; confidence_ = 0;
        status_ = QStringLiteral("Snímání je příliš pomalé — vstup pozastaven");
        emit frameReady(frame.image); emit changed(); return;
    }
    const double interval = now - lastFrame_;
    if (interval > 0) fps_ = fps_ == 0 ? 1 / interval : fps_ * 0.8 + 0.2 / interval;
    lastFrame_ = now;
    handVisible_ = frame.hand.has_value();
    confidence_ = frame.hand ? frame.hand->confidence : 0;
    inferenceMs_ = frame.inferenceMs;
    const auto command = engine_.update(frame.hand, frame.timestamp, settings_->snapshot());
    status_ = engine_.paused() ? QStringLiteral("Pozastaveno")
        : !handVisible_ ? QStringLiteral("Hledám ruku…") : QString::fromUtf8(handmouse::GestureEngine::poseName(command.pose));
    if (!settings_->snapshot().previewOnly && input_.ready() && !input_.apply(command)) {
        error_ = input_.error(); stop();
    }
    emit frameReady(frame.image); emit changed();
}
void Controller::settingsChanged() {
    const auto settings = settings_->snapshot();
    input_.release(); engine_.reset();
    if (settings.camera != previousSettings_.camera && worker_.isRunning()) stop();
    if (settings.previewOnly) input_.close();
    else if (running_ && !input_.open()) {
        error_ = input_.error();
        stop();
    }
    previousSettings_ = settings;
    revision_ = worker_.configure(settings);
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
        cameras_.append(QVariantMap{{"label", label}, {"index", index}});
    }
    emit camerasChanged();
}
