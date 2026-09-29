#include "camera_worker.hpp"
#include "camera_device.hpp"
#include "hand_tracker.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <QDebug>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <cmath>

namespace {
struct CapturedFrame {
    cv::Mat image;
    handmouse::Settings config;
    quint64 revision = 0;
    double timestamp = 0, cameraFps = 0, waitMs = 0, decodeMs = 0;
};
// The capture thread replaces this single slot; inference never drains a FIFO.
struct LatestFrame {
    std::mutex mutex;
    std::condition_variable ready;
    std::optional<CapturedFrame> frame;
    quint64 skipped = 0;
    bool done = false;
};
struct JoinProcessor {
    LatestFrame& latest;
    std::thread& thread;
    ~JoinProcessor() {
        { std::lock_guard<std::mutex> lock(latest.mutex); latest.done = true; latest.frame.reset(); }
        latest.ready.notify_one();
        thread.join();
    }
};
}
CameraWorker::~CameraWorker() { requestInterruption(); wait(); }
quint64 CameraWorker::configure(const handmouse::Settings& settings) {
    std::lock_guard<std::mutex> lock(mutex_); settings_ = settings; return ++revision_;
}
double CameraWorker::now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
void CameraWorker::run() {
    framePending_ = false;
    try {
        HandTracker tracker;
        tracker.load(modelDirectory_.toStdString());
        if (isInterruptionRequested()) return;
        handmouse::Settings config;
        { std::lock_guard<std::mutex> lock(mutex_); config = settings_; }
        const auto capabilities = cameraCapabilities(config.camera);
        const auto mode = chooseCameraMode(capabilities.modes, config.cameraFps, config.cameraWidth, config.cameraHeight);
        cv::VideoCapture camera;
        if (!camera.open(config.camera, cv::CAP_V4L2)) {
            emit failed(QStringLiteral("Kameru %1 nelze otevřít. Připoj ji a ověř, že ji nepoužívá jiná aplikace.").arg(config.camera));
            return;
        }
        const int width = mode ? mode->width : (config.cameraWidth ? config.cameraWidth : 640), height = mode ? mode->height : (config.cameraHeight ? config.cameraHeight : 480);
        const double requestedRate = mode ? mode->fps : config.cameraFps;
        const int format = mode ? int(mode->format) : cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        const bool formatSet = camera.set(cv::CAP_PROP_FOURCC, format);
        const bool widthSet = camera.set(cv::CAP_PROP_FRAME_WIDTH, width);
        const bool heightSet = camera.set(cv::CAP_PROP_FRAME_HEIGHT, height);
        const bool fpsSet = camera.set(cv::CAP_PROP_FPS, requestedRate);
        // Two driver buffers allow acquisition during dequeue/decode. A single buffer halves FPS on UVC cameras.
        const bool bufferSet = camera.set(cv::CAP_PROP_BUFFERSIZE, 2);
        CameraExposure exposure(config.camera);
        // V4L2 starts streaming on the first grab; waitAny alone does not start it.
        if (!camera.grab()) {
            emit failed(QStringLiteral("Kamera neposlala úvodní snímek.")); return;
        }
        const int actualWidth = int(camera.get(cv::CAP_PROP_FRAME_WIDTH));
        const int actualHeight = int(camera.get(cv::CAP_PROP_FRAME_HEIGHT));
        const double actualFps = camera.get(cv::CAP_PROP_FPS);
        const auto actualFormat = uint32_t(camera.get(cv::CAP_PROP_FOURCC));
        QString description = QStringLiteral("%1 × %2 · %3 · režim %4 fps · požadováno %5 fps")
            .arg(actualWidth).arg(actualHeight).arg(QString::fromStdString(cameraFormatName(actualFormat)))
            .arg(actualFps, 0, 'f', 1).arg(config.cameraFps);
        if (config.cameraWidth && (actualWidth != config.cameraWidth || actualHeight != config.cameraHeight))
            description += QStringLiteral(". Požadované rozlišení %1 × %2 není dostupné; používá se uvedený režim.")
                .arg(config.cameraWidth).arg(config.cameraHeight);
        if (std::abs(actualFps - config.cameraFps) > 0.5)
            description += QStringLiteral(". Požadované FPS nejsou dostupné; používá se uvedený režim.");
        if (!formatSet || !widthSet || !heightSet || !fpsSet || !bufferSet ||
            actualWidth != width || actualHeight != height || actualFormat != uint32_t(format))
            description += QStringLiteral(" Některé parametry kamera nepřijala; zobrazeny jsou skutečné hodnoty.");
        if (!exposure.fixedFrameRate())
            description += QStringLiteral(" Pevnou frekvenci expozice nelze ověřit; za šera mohou FPS klesnout.");
        qInfo().noquote() << description;
        emit modeReady(description, actualFps);
        emit cameraOpened();

        LatestFrame latest;
        std::atomic<bool> processingFailed{false};
        QString processingError;
        std::thread processor([&] {
            try {
                bool lastMirror = config.mirror;
                double lastProcessed = 0;
                while (true) {
                    CapturedFrame captured;
                    quint64 skipped;
                    {
                        std::unique_lock<std::mutex> lock(latest.mutex);
                        latest.ready.wait(lock, [&] { return latest.done || latest.frame.has_value(); });
                        if (latest.done) break;
                        captured = std::move(*latest.frame); latest.frame.reset();
                        if (framePending_.load()) { ++latest.skipped; continue; }
                        skipped = latest.skipped;
                    }
                    if (isInterruptionRequested()) break;
                    if (lastMirror != captured.config.mirror || captured.timestamp - lastProcessed > 0.25) tracker.reset();
                    lastMirror = captured.config.mirror; lastProcessed = captured.timestamp;
                    auto& frame = captured.image;
                    if (captured.config.mirror) cv::flip(frame, frame, 1);
                    const double start = now();
                    auto hand = tracker.track(frame, captured.config.confidence);
                    const double inference = (now() - start) * 1000;
                    const double previewStart = now();
                    if (captured.config.skeleton && hand) HandTracker::draw(frame, *hand);
                    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
                    QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_RGB888);
                    TrackingFrame result;
                    result.image = image.copy(); result.hand = hand; result.revision = captured.revision;
                    result.timestamp = captured.timestamp; result.inferenceMs = inference;
                    result.cameraFps = captured.cameraFps; result.waitMs = captured.waitMs; result.decodeMs = captured.decodeMs;
                    result.previewMs = (now() - previewStart) * 1000; result.skippedFrames = skipped;
                    if (isInterruptionRequested()) break;
                    framePending_ = true;
                    emit frameReady(result);
                }
            } catch (const std::exception& e) {
                processingError = QString::fromUtf8(e.what()); processingFailed = true;
            }
        });
        {
            JoinProcessor join{latest, processor};
            int emptyFrames = 0, captures = 0;
            double lastCapture = now(), rateStart = lastCapture, captureFps = 0;
            const std::vector<cv::VideoCapture> streams{camera};
            bool firstFrame = true;
            while (!isInterruptionRequested() && !processingFailed.load()) {
                CapturedFrame captured;
                { std::lock_guard<std::mutex> lock(mutex_); captured.config = settings_; captured.revision = revision_; }
                std::vector<int> ready;
                const double waitStart = now();
                if (!firstFrame && !cv::VideoCapture::waitAny(streams, ready, 100000000)) {
                    if (now() - lastCapture > 2) throw std::runtime_error("Kamera neodpovídá. Zkus ji odpojit a znovu připojit.");
                    continue;
                }
                captured.timestamp = now();
                captured.waitMs = (captured.timestamp - waitStart) * 1000;
                firstFrame = false;
                if (isInterruptionRequested()) break;
                if (!camera.retrieve(captured.image) || captured.image.empty()) {
                    if (++emptyFrames >= 5) throw std::runtime_error("Kamera přestala posílat obraz. Zkontroluj připojení.");
                    continue;
                }
                captured.decodeMs = (now() - captured.timestamp) * 1000;
                emptyFrames = 0; lastCapture = captured.timestamp;
                ++captures;
                if (lastCapture - rateStart >= 1) {
                    captureFps = (captures - 1) / (lastCapture - rateStart);
                    captures = 1; rateStart = lastCapture;
                }
                captured.cameraFps = captureFps;
                {
                    std::lock_guard<std::mutex> lock(latest.mutex);
                    if (latest.frame) ++latest.skipped;
                    latest.frame = std::move(captured);
                }
                latest.ready.notify_one();
            }
        }
        if (processingFailed) throw std::runtime_error(processingError.toStdString());
    } catch (const std::exception& e) {
        emit failed(QStringLiteral("Snímání se zastavilo: %1").arg(QString::fromUtf8(e.what())));
    }
}
