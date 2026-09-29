#pragma once
#include "gestures/gesture_engine.hpp"
#include <QThread>
#include <QImage>
#include <QString>
#include <atomic>
#include <mutex>

struct TrackingFrame {
    QImage image;
    std::optional<handmouse::Hand> hand;
    quint64 revision = 0;
    double timestamp = 0;
    double inferenceMs = 0;
    double cameraFps = 0;
    double waitMs = 0;
    double decodeMs = 0;
    double previewMs = 0;
    quint64 skippedFrames = 0;
};
Q_DECLARE_METATYPE(TrackingFrame)

class CameraWorker : public QThread {
    Q_OBJECT
public:
    explicit CameraWorker(QObject* parent = nullptr) : QThread(parent) {}
    ~CameraWorker() override;
    quint64 configure(const handmouse::Settings& settings);
    void setModelDirectory(QString directory) { modelDirectory_ = std::move(directory); }
    void acknowledge() { framePending_ = false; }
    static double now();
signals:
    void frameReady(const TrackingFrame& frame);
    void cameraOpened();
    void modeReady(const QString& description, double negotiatedFps);
    void failed(const QString& error);
protected:
    void run() override;
private:
    std::mutex mutex_;
    handmouse::Settings settings_;
    QString modelDirectory_;
    quint64 revision_ = 0;
    std::atomic<bool> framePending_{false};
};
