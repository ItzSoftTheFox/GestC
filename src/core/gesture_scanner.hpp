#pragma once
#include "mouse_controller.hpp"
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp> 
#include <QObject>
#include <QTimer>
#include <QImage>
#include <vector>

struct HandLandmark {
    float x;
    float y;
    float z;
};

class GestureScanner : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)

    // Define UI-accessible properties for calibration
    Q_PROPERTY(float offsetX READ offsetX WRITE setOffsetX NOTIFY offsetXChanged)
    Q_PROPERTY(float offsetY READ offsetY WRITE setOffsetY NOTIFY offsetYChanged)
    Q_PROPERTY(float cropMultiplier READ cropMultiplier WRITE setCropMultiplier NOTIFY cropMultiplierChanged)

    // property for sensitivity scaling
    Q_PROPERTY(float movementScale READ movementScale WRITE setMovementScale NOTIFY movementScaleChanged)

    // Add property for the smoothing factor
    Q_PROPERTY(float smoothingFactor READ smoothingFactor WRITE setSmoothingFactor NOTIFY smoothingFactorChanged)
    // Property to define how close fingers must be to trigger a click
    Q_PROPERTY(float clickThreshold READ clickThreshold WRITE setClickThreshold NOTIFY clickThresholdChanged)
    // Add property for joystick scroll sensitivity
    Q_PROPERTY(float scrollSensitivity READ scrollSensitivity WRITE setScrollSensitivity NOTIFY scrollSensitivityChanged)

public:
    explicit GestureScanner(QObject *parent = nullptr);
    ~GestureScanner();

    Q_INVOKABLE bool startCamera();
    Q_INVOKABLE void stopCamera();

    bool isScanning() const;

    // Getters for the calibration values
    float offsetX() const;
    float offsetY() const;
    float cropMultiplier() const;
    float movementScale() const;
    float smoothingFactor() const;
    float clickThreshold() const;
    float scrollSensitivity() const;

    // Setters for the calibration values
    void setOffsetX(float value);
    void setOffsetY(float value);
    void setCropMultiplier(float value);
    void setMovementScale(float value);
    void setSmoothingFactor(float value);
    void setClickThreshold(float value);
    void setScrollSensitivity(float value);

signals:
    void frameReady(const QImage &frame);
    void isScanningChanged();

    // Signals to notify the UI when values change
    void offsetXChanged();
    void offsetYChanged();
    void cropMultiplierChanged();
    void movementScaleChanged();
    void smoothingFactorChanged();
    void clickThresholdChanged();
    void scrollSensitivityChanged();

private slots:
    void processFrame();

private:
    cv::VideoCapture cap;
    QTimer *timer;
    std::vector<HandLandmark> currentLandmarks;

    MouseController* mouse;
    
    cv::dnn::Net palmNet;
    cv::dnn::Net landmarkNet;
    
    // Vector to store the 2016 static anchor coordinates
    std::vector<cv::Point2f> anchors;//Expand the bounding box by 50 percent to include extended fingers
                
    // Private variables holding the actual calibration data
    float m_offsetX;
    float m_offsetY;
    float m_cropMultiplier;
    float m_movementScale;
    
    // Variables for the EMA filter
    float m_smoothingFactor;
    float m_smoothedX;
    float m_smoothedY;
    bool m_isFirstFrame;

    bool m_isLeftClicked;
    float m_clickThreshold;
    bool m_isScanning;
    int m_releaseFrameCounter;

    // Variables for joystick scroll and gesture state management
    bool m_isScrolling;
    float m_scrollAnchorY;
    float m_scrollSensitivity;
    int m_scrollReleaseCounter;
    float m_scrollAccumulator;

    void detectHand(cv::Mat& frame);
    void drawLandmarks(cv::Mat& frame);
    bool initModel();
    void processLandmarks(const cv::Mat& crop, const cv::Rect& box, int frameWidth, int frameHeight);
    
    void generateAnchors();
};