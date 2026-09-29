#pragma once
#include "settings.hpp"
#include "tracking/camera_worker.hpp"
#include "platform/input_device.hpp"
#include <QTimer>
#include <QVariantList>

class Controller : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool running READ running NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool paused READ paused NOTIFY changed)
    Q_PROPERTY(bool handVisible READ handVisible NOTIFY changed)
    Q_PROPERTY(bool inputReady READ inputReady NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(double fps READ fps NOTIFY changed)
    Q_PROPERTY(double cameraFps READ cameraFps NOTIFY changed)
    Q_PROPERTY(double negotiatedFps READ negotiatedFps NOTIFY changed)
    Q_PROPERTY(QString cameraMode READ cameraMode NOTIFY changed)
    Q_PROPERTY(QString backend READ backend CONSTANT)
    Q_PROPERTY(QVariantList resolutionOptions READ resolutionOptions NOTIFY camerasChanged)
    Q_PROPERTY(QVariantList fpsOptions READ fpsOptions NOTIFY camerasChanged)
    Q_PROPERTY(double latencyMs READ latencyMs NOTIFY changed)
    Q_PROPERTY(double renderMs READ renderMs NOTIFY changed)
    Q_PROPERTY(double inferenceMs READ inferenceMs NOTIFY changed)
    Q_PROPERTY(double confidence READ confidence NOTIFY changed)
    Q_PROPERTY(QVariantList cameras READ cameras NOTIFY camerasChanged)
public:
    Controller(AppSettings* settings, QString modelDirectory, QObject* parent = nullptr);
    ~Controller() override;
    bool running() const { return running_; }
    bool busy() const { return busy_; }
    bool paused() const { return engine_.paused(); }
    bool handVisible() const { return handVisible_; }
    bool inputReady() const { return input_.ready(); }
    QString status() const { return status_; }
    QString error() const { return error_; }
    double fps() const { return fps_; }
    double cameraFps() const { return cameraFps_; }
    double negotiatedFps() const { return negotiatedFps_; }
    QString cameraMode() const { return cameraMode_; }
    QString backend() const { return QStringLiteral("CPU · OpenCV"); }
    QVariantList resolutionOptions() const { return resolutionOptions_; }
    QVariantList fpsOptions() const { return fpsOptions_; }
    double latencyMs() const { return latencyMs_; }
    double renderMs() const { return renderMs_; }
    void reportRender(double ms) { renderMs_ = ms; }
    double inferenceMs() const { return inferenceMs_; }
    double confidence() const { return confidence_; }
    QVariantList cameras() const { return cameras_; }
    Q_INVOKABLE void start();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void refreshCameras();
    Q_INVOKABLE void clearError() { error_.clear(); emit changed(); }
    Q_INVOKABLE void emergencyStop();
signals:
    void changed();
    void camerasChanged();
    void frameReady(const QImage& image);
private:
    void receive(const TrackingFrame& frame);
    void settingsChanged();
    void refreshModes();
    AppSettings* settings_;
    handmouse::Settings previousSettings_;
    CameraWorker worker_;
    quint64 revision_ = 0;
    handmouse::GestureEngine engine_;
    InputDevice input_;
    QTimer watchdog_;
    QVariantList cameras_, fpsOptions_, resolutionOptions_;
    bool restartRequested_ = false;
    QString cameraMode_;
    double cameraFps_ = 0, negotiatedFps_ = 0, latencyMs_ = 0, renderMs_ = 0;
    double fpsWindow_ = 0;
    int fpsFrames_ = 0;
    bool running_ = false, busy_ = false, stopping_ = false, handVisible_ = false;
    QString status_ = QStringLiteral("Připraveno"), error_;
    double fps_ = 0, inferenceMs_ = 0, confidence_ = 0, lastFrame_ = 0;
};
