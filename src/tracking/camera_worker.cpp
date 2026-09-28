#include "camera_worker.hpp"
#include "hand_tracker.hpp"
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <chrono>

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
        cv::VideoCapture camera;
        if (!camera.open(config.camera, cv::CAP_V4L2)) {
            emit failed(QStringLiteral("Kameru %1 nelze otevřít. Připoj ji a ověř, že ji nepoužívá jiná aplikace.").arg(config.camera));
            return;
        }
        camera.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        camera.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        camera.set(cv::CAP_PROP_FPS, 30);
        camera.set(cv::CAP_PROP_BUFFERSIZE, 1);
        // V4L2 starts streaming on the first grab; waitAny alone does not start it.
        if (!camera.grab()) {
            emit failed(QStringLiteral("Kamera neposlala úvodní snímek.")); return;
        }
        emit cameraOpened();
        bool lastMirror = config.mirror;
        int emptyFrames = 0;
        quint64 revision = 0;
        double lastCapture = now();
        const std::vector<cv::VideoCapture> streams{camera};
        bool firstFrame = true;
        while (!isInterruptionRequested()) {
            { std::lock_guard<std::mutex> lock(mutex_); config = settings_; revision = revision_; }
            std::vector<int> ready;
            if (!firstFrame && !cv::VideoCapture::waitAny(streams, ready, 100000000)) {
                if (now() - lastCapture > 2) { emit failed(QStringLiteral("Kamera neodpovídá. Zkus ji odpojit a znovu připojit.")); break; }
                continue;
            }
            firstFrame = false;
            if (isInterruptionRequested()) break;
            cv::Mat frame;
            if (!camera.retrieve(frame) || frame.empty()) {
                if (++emptyFrames >= 5) { emit failed(QStringLiteral("Kamera přestala posílat obraz. Zkontroluj připojení.")); break; }
                msleep(20); continue;
            }
            emptyFrames = 0;
            lastCapture = now();
            if (framePending_.load()) continue; // Never build up a queue of stale input.
            if (lastMirror != config.mirror) tracker.reset();
            lastMirror = config.mirror;
            if (config.mirror) cv::flip(frame, frame, 1);
            const double start = now();
            auto hand = tracker.track(frame, config.confidence);
            const double inference = (now() - start) * 1000;
            if (config.skeleton && hand) HandTracker::draw(frame, *hand);
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            QImage image(frame.data, frame.cols, frame.rows, static_cast<int>(frame.step), QImage::Format_RGB888);
            if (isInterruptionRequested()) break;
            framePending_ = true;
            emit frameReady(TrackingFrame{image.copy(), hand, revision, start, inference});
        }
    } catch (const std::exception& e) {
        emit failed(QStringLiteral("Snímání se zastavilo: %1").arg(QString::fromUtf8(e.what())));
    }
}
