#include "gesture_scanner.hpp"

GestureScanner::GestureScanner(QObject *parent) : QObject(parent), 
m_isScanning(false), m_offsetX(0.0f), m_offsetY(0.0f), m_cropMultiplier(2.0f), m_movementScale(1.5f), 
m_smoothingFactor(0.2f), m_clickThreshold(0.05f), m_isFirstFrame(true), m_isLeftClicked(false),
m_releaseFrameCounter(0), m_isRightClicked(false), m_rightReleaseFrameCounter(0),
m_isScrolling(false), m_scrollAnchorY(0.0f), m_scrollSensitivity(15.0f),
m_isPaused(false), m_pauseCooldown(0), m_pauseFrames(0) {
    
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &GestureScanner::processFrame);
    
    mouse = new MouseController(1920, 1080);
    
    initModel();
    generateAnchors();
}

// Implement calibration getters 
float GestureScanner::offsetX() const { return m_offsetX; }
float GestureScanner::offsetY() const { return m_offsetY; }
float GestureScanner::cropMultiplier() const { return m_cropMultiplier; }
float GestureScanner::movementScale() const { return m_movementScale; }
float GestureScanner::smoothingFactor() const { return m_smoothingFactor; }
float GestureScanner::clickThreshold() const { return m_clickThreshold; }
float GestureScanner::scrollSensitivity() const { return m_scrollSensitivity; }

// ----------SETTERs--------------

void GestureScanner::setCropMultiplier(float value) {
    if (m_cropMultiplier == value) return;
    m_cropMultiplier = value;
    emit cropMultiplierChanged();
}

void GestureScanner::setScrollSensitivity(float value) {
    if (m_scrollSensitivity == value) return;
    m_scrollSensitivity = value;
    emit scrollSensitivityChanged();
}

// calibration setters with signal emission
void GestureScanner::setOffsetX(float value) {
    if (m_offsetX == value) return;
    m_offsetX = value;
    emit offsetXChanged();
}

void GestureScanner::setOffsetY(float value) {
    if (m_offsetY == value) return;
    m_offsetY = value;
    emit offsetYChanged();
}


void GestureScanner::setSmoothingFactor(float value) {
    if (m_smoothingFactor == value) return;
    m_smoothingFactor = value;
    emit smoothingFactorChanged();
}

void GestureScanner::setMovementScale(float value) {
    if (m_movementScale == value) return;
    m_movementScale = value;
    emit movementScaleChanged();
}

void GestureScanner::setClickThreshold(float value) {
    if (m_clickThreshold == value) return;
    m_clickThreshold = value;
    emit clickThresholdChanged();
}

//---------------------------------------------------

GestureScanner::~GestureScanner() {
    // Bezpečné uvolnění hardwarových prostředků při ukončení programu
    stopCamera();
    delete mouse;
}

bool GestureScanner::startCamera() {
    if (cap.isOpened()) return true;

    // Vynucení linuxového ovladače V4L2 pro rychlý a spolehlivý start
    cap.open(0, cv::CAP_V4L2);
    if (!cap.isOpened()) {
        std::cerr << "Chyba: Nelze otevrit kameru." << std::endl;
        return false;
    }
    
    // Čtení nového snímku každých 33 milisekund
    timer->start(33);
    
    // Aktualizace stavu indikátoru pro QML rozhraní
    m_isScanning = true;
    emit isScanningChanged();
    m_isFirstFrame = true;

    return true;
}

void GestureScanner::stopCamera() {
    timer->stop();
    if (cap.isOpened()) {
        cap.release();
    }
    
    // Aktualizace stavu indikátoru pro QML rozhraní
    m_isScanning = false;
    emit isScanningChanged();
}

bool GestureScanner::isScanning() const {
    return m_isScanning;
}

void GestureScanner::processFrame() {
    if (!cap.isOpened()) return;

    cv::Mat frame;
    cap >> frame;
    
    if (frame.empty()) return;

    // Flip the image horizontally for natural user interaction.
    // The value 1 means flipping around the y-axis.
    cv::flip(frame, frame, 1);

    detectHand(frame);
    drawLandmarks(frame);
    
    cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
    QImage qimg((uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
    
    emit frameReady(qimg.copy());
}

bool GestureScanner::initModel() {
    try {
        // Use the exact filenames from your screenshot
        std::string palmPath = "/home/Fox/Plocha/Projects/GestC/models/hand_detector.tflite";
        std::string landmarkPath = "/home/Fox/Plocha/Projects/GestC/models/hand_landmarks_detector.tflite";
        
        // Load the hand bounding box detector
        palmNet = cv::dnn::readNet(palmPath);
        palmNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        palmNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);

        // Load the detailed 21-point landmark detector
        landmarkNet = cv::dnn::readNet(landmarkPath);
        landmarkNet.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        landmarkNet.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        
        std::cout << "Both neural networks loaded successfully." << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "Failed to load models: " << e.what() << std::endl;
        return false;
    }
}

bool GestureScanner::isPaused() const { 
    return m_isPaused; 
}

void GestureScanner::togglePause() {
    m_isPaused = !m_isPaused;
    emit isPausedChanged();
    
    // Safety cleanup when pausing manually
    if (m_isPaused && mouse) {
        if (m_isLeftClicked) {
            mouse->click(false);
            m_isLeftClicked = false;
        }
        m_isScrolling = false;
        m_scrollAccumulator = 0.0f;
    }
}

void GestureScanner::detectHand(cv::Mat& frame) {
    if (palmNet.empty() || landmarkNet.empty()) return;

    // --- PHASE 1: FIND THE HAND IN THE ROOM ---
    
    cv::Mat palmBlob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, cv::Size(192, 192), cv::Scalar(), true, false);
    palmNet.setInput(palmBlob);
    
    std::vector<cv::Mat> palmOutputs;
    palmNet.forward(palmOutputs, palmNet.getUnconnectedOutLayersNames());

    currentLandmarks.clear();

    if (!palmOutputs.empty()) {
        // Tensor 0 contains the confidence scores. Tensor 1 contains the spatial coordinates.
        // We need to identify the exact index of the classification tensor.
        // Usually, the smaller tensor (2016 elements) is the score, and the larger (2016 * 18) is the bounding box.
        
        int scoreTensorIndex = (palmOutputs[0].total() < palmOutputs[1].total()) ? 0 : 1;
        cv::Mat scoreTensor = palmOutputs[scoreTensorIndex];
        
        // Get a direct pointer to the 2016 confidence scores
        float* scores = scoreTensor.ptr<float>();
        
        // Find the index of the anchor box with the highest confidence
        int bestIndex = 0;
        float maxScore = -1.0f;
        
        for (int i = 0; i < 2016; ++i) {
            // Apply a sigmoid function to the raw score to get a proper probability
            // Some TFLite models output raw logits instead of probabilities
            float currentScore = 1.0f / (1.0f + std::exp(-scores[i]));
            
            if (currentScore > maxScore) {
                maxScore = currentScore;
                bestIndex = i;
            }
        }
        
        // Check if the confidence score is high enough to process
        if (maxScore > 0.5f) {
            int boxTensorIndex = (scoreTensorIndex == 0) ? 1 : 0;
            cv::Mat boxTensor = palmOutputs[boxTensorIndex];
            float* boxData = boxTensor.ptr<float>();
            int dataOffset = bestIndex * 18;
            
            float dx = boxData[dataOffset];
            float dy = boxData[dataOffset + 1];
            float boxWidth = boxData[dataOffset + 2];
            float boxHeight = boxData[dataOffset + 3];
            
            // Decode the bounding box using the pre-calculated anchor
            // Divide raw deltas by the input image size (192.0)
            float centerX = (dx / 192.0f) + anchors[bestIndex].x;
            float centerY = (dy / 192.0f) + anchors[bestIndex].y;
            float width = boxWidth / 192.0f;
            float height = boxHeight / 192.0f;
            
            // Convert normalized coordinates (0.0 - 1.0) to actual frame pixels
            int pixelX = static_cast<int>((centerX - width / 2.0f) * frame.cols);
            int pixelY = static_cast<int>((centerY - height / 2.0f) * frame.rows);
            int pixelWidth = static_cast<int>(width * frame.cols);
            int pixelHeight = static_cast<int>(height * frame.rows);
            
            // Create a safe bounding box within the frame limits
            cv::Rect handBox(pixelX, pixelY, pixelWidth, pixelHeight);
            handBox &= cv::Rect(0, 0, frame.cols, frame.rows);
            
            // Draw a blue rectangle around the detected palm for visual confirmation
            if (handBox.area() > 0) {
                cv::rectangle(frame, handBox, cv::Scalar(255, 0, 0), 3);

                // Calculate the horizontal center normally
                int centerX = handBox.x + handBox.width / 2;
                
                // Shift the vertical focal center UP by 30% of the box height.
                // This targets the fingers directly instead of the wrist base.
                int centerY = (handBox.y + handBox.height / 2) - static_cast<int>(handBox.height * 0.3f);

                // Find the longest edge to maintain aspect ratio
                int maxDimension = std::max(handBox.width, handBox.height);

                // Apply the dynamic crop multiplier from the UI
                int squareSize = static_cast<int>(maxDimension * m_cropMultiplier);

                cv::Rect squareBox(
                    centerX - squareSize / 2,
                    centerY - squareSize / 2,
                    squareSize,
                    squareSize
                );

                // Ensure the new square does not cross the physical limits of the camera frame
                squareBox &= cv::Rect(0, 0, frame.cols, frame.rows);

                // Proceed only if the crop area is geometrically valid
                if (squareBox.area() > 0) {
                    cv::rectangle(frame, squareBox, cv::Scalar(0, 255, 255), 2);

                    cv::Mat handCrop = frame(squareBox);
                    processLandmarks(handCrop, squareBox, frame.cols, frame.rows);
                }
            }
        }
    }
}

void GestureScanner::drawLandmarks(cv::Mat& frame) {
    if (currentLandmarks.empty()) return;

    // Convert normalized coordinates to exact pixel coordinates
    std::vector<cv::Point> pixelPoints;
    for (const auto& lm : currentLandmarks) {
        int px = static_cast<int>(lm.x * frame.cols);
        int py = static_cast<int>(lm.y * frame.rows);
        pixelPoints.push_back(cv::Point(px, py));
    }

    // MediaPipe Hand Skeleton connections mapping
    std::vector<std::pair<int, int>> connections = {
        // Thumb
        {0, 1}, {1, 2}, {2, 3}, {3, 4},
        // Index finger
        {0, 5}, {5, 6}, {6, 7}, {7, 8},
        // Middle finger
        {9, 10}, {10, 11}, {11, 12},
        // Ring finger
        {13, 14}, {14, 15}, {15, 16},
        // Pinky finger
        {0, 17}, {17, 18}, {18, 19}, {19, 20},
        // Palm web connections
        {5, 9}, {9, 13}, {13, 17}
    };

    // Draw the structural lines first so they sit under the joints
    for (const auto& conn : connections) {
        // Ensure both points are within the bounds of our extracted array to prevent crashes
        if (conn.first < pixelPoints.size() && conn.second < pixelPoints.size()) {
            cv::Point p1 = pixelPoints[conn.first];
            cv::Point p2 = pixelPoints[conn.second];
            
            // Draw a thin blue line for the bones
            cv::line(frame, p1, p2, cv::Scalar(255, 0, 0), 2);
        }
    }

    // Draw the joints on top of the lines
    for (const auto& pt : pixelPoints) {
        // Ensure point is within physical frame bounds before drawing
        if (pt.x >= 0 && pt.x < frame.cols && pt.y >= 0 && pt.y < frame.rows) {
            // Draw a solid green circle for the joint
            cv::circle(frame, pt, 5, cv::Scalar(0, 255, 0), cv::FILLED);
        }
    }
}

void GestureScanner::generateAnchors() {
    anchors.clear();
    
    // Generate the 24x24 grid (stride 8). Each cell has 2 anchors.
    for (int y = 0; y < 24; ++y) {
        for (int x = 0; x < 24; ++x) {
            float cx = (x + 0.5f) / 24.0f;
            float cy = (y + 0.5f) / 24.0f;
            anchors.push_back(cv::Point2f(cx, cy));
            anchors.push_back(cv::Point2f(cx, cy));
        }
    }
    
    // Generate the 12x12 grid (stride 16). Each cell has 6 anchors.
    for (int y = 0; y < 12; ++y) {
        for (int x = 0; x < 12; ++x) {
            float cx = (x + 0.5f) / 12.0f;
            float cy = (y + 0.5f) / 12.0f;
            for (int i = 0; i < 6; ++i) {
                anchors.push_back(cv::Point2f(cx, cy));
            }
        }
    }
}

void GestureScanner::processLandmarks(const cv::Mat& crop, const cv::Rect& box, int frameWidth, int frameHeight) {
    // 1. Prepare and run the neural network
    cv::Mat blob = cv::dnn::blobFromImage(crop, 1.0 / 255.0, cv::Size(256, 256), cv::Scalar(), true, false);
    landmarkNet.setInput(blob);
    
    std::vector<cv::Mat> outputs;
    landmarkNet.forward(outputs, landmarkNet.getUnconnectedOutLayersNames());

    cv::Mat landmarkTensor;
    
    // 2. Filter outputs to find the correct 2D image landmarks
    for (const auto& mat : outputs) {
        if (mat.total() == 63) {
            const float* data = mat.ptr<float>();
            if (std::abs(data[0]) > 0.001f || std::abs(data[1]) > 0.001f) {
                landmarkTensor = mat;
                break;
            }
        }
    }

    if (!landmarkTensor.empty()) {
        float* data = landmarkTensor.ptr<float>();

        //------------------------------------------------------------------------------

        // Extract coordinates for cursor movement (Middle finger base)
        float rawPalmX = data[9 * 3];
        float rawPalmY = data[9 * 3 + 1];
        float rawPalmZ = data[9 * 3 + 2];
            
        // Extract coordinates for gesture recognition
        float rawThumbX = data[4 * 3];
        float rawThumbY = data[4 * 3 + 1];
        float rawThumbZ = data[4 * 3 + 2];

        float rawIndexX = data[8 * 3];
        float rawIndexY = data[8 * 3 + 1];
        float rawIndexZ = data[8 * 3 + 2];

        // Extract coordinates for the middle finger tip (12) and PIP joint (10) for scrolling
        float rawMidTipX = data[12 * 3];
        float rawMidTipY = data[12 * 3 + 1];
        float rawMidTipZ = data[12 * 3 + 2];

        float rawMidPipX = data[10 * 3];
        float rawMidPipY = data[10 * 3 + 1];
        float rawMidPipZ = data[10 * 3 + 2];

        float rawWristX = data[0 * 3];
        float rawWristY = data[0 * 3 + 1];
        float rawWristZ = data[0 * 3 + 2];

        // Extract coordinates for the ring finger tip (16) and MCP joint (13)
        float rawRingTipX = data[16 * 3];
        float rawRingTipY = data[16 * 3 + 1];
        float rawRingTipZ = data[16 * 3 + 2];

        float rawRingMcpX = data[13 * 3];
        float rawRingMcpY = data[13 * 3 + 1];
        float rawRingMcpZ = data[13 * 3 + 2];

        // Extract coordinates for the pinky finger tip (20) and MCP joint (17)
        float rawPinkyTipX = data[20 * 3];
        float rawPinkyTipY = data[20 * 3 + 1];
        float rawPinkyTipZ = data[20 * 3 + 2];

        float rawPinkyMcpX = data[17 * 3];
        float rawPinkyMcpY = data[17 * 3 + 1];
        float rawPinkyMcpZ = data[17 * 3 + 2];

        //------------------------------------------------------------------------------

        // Normalize palm coordinates for screen movement
        float localPalmX = rawPalmX;
        float localPalmY = rawPalmY;
        if (rawPalmX > 1.0f || rawPalmY > 1.0f || rawPalmX < -1.0f || rawPalmY < -1.0f) {
            localPalmX /= 256.0f;
            localPalmY /= 256.0f;
        }

        // Apply UI calibration offsets
        localPalmX += m_offsetX;
        localPalmY += m_offsetY;

        // Map local crop coordinates back to global camera frame
        float globalX = box.x + (localPalmX * box.width);
        float globalY = box.y + (localPalmY * box.height);

        // Store ALL 21 coordinates for skeleton rendering and calculation
        currentLandmarks.clear();
        for (int i = 0; i < 21; ++i) {
            // Extract raw landmark data
            float rawX = data[i * 3];
            float rawY = data[i * 3 + 1];
            float rawZ = data[i * 3 + 2];
            
            // Normalize coordinates
            float localX = rawX;
            float localY = rawY;
            if (rawX > 1.0f || rawY > 1.0f || rawX < -1.0f || rawY < -1.0f) {
                localX /= 256.0f;
                localY /= 256.0f;
            }
            
            // Map local crop coordinates back to global camera frame
            float globalLandmarkX = box.x + (localX * box.width);
            float globalLandmarkY = box.y + (localY * box.height);
            
            HandLandmark mark;
            mark.x = globalLandmarkX / static_cast<float>(frameWidth);
            mark.y = globalLandmarkY / static_cast<float>(frameHeight);
            mark.z = rawZ; // Keep raw Z for depth calculations
            
            currentLandmarks.push_back(mark);
        }
        
        // Use the middle finger base (index 9) for cursor movement as before
        HandLandmark palmMark = currentLandmarks[9];
        // Apply calibration offsets strictly for the cursor movement
        palmMark.x += (m_offsetX / static_cast<float>(frameWidth));
        palmMark.y += (m_offsetY / static_cast<float>(frameHeight));

        // Execute system actions if the virtual mouse is ready
        if (mouse) {
            
            // --- GESTURE MATH PREPARATION ---

            // Calculate the reference size of the palm (Wrist to Middle Finger Base)
            float refDx = rawPalmX - rawWristX;
            float refDy = rawPalmY - rawWristY;
            float refDz = rawPalmZ - rawWristZ;
            float palmSize = std::sqrt((refDx * refDx) + (refDy * refDy) + (refDz * refDz));

            // Calculate the actual 3D distance between thumb and index finger
            float pinchDx = rawIndexX - rawThumbX;
            float pinchDy = rawIndexY - rawThumbY;
            float pinchDz = rawIndexZ - rawThumbZ;
            float pinchDistance = std::sqrt((pinchDx * pinchDx) + (pinchDy * pinchDy) + (pinchDz * pinchDz));

            // Create a scale-invariant ratio for clicking
            float pinchRatio = 1.0f;
            if (palmSize > 0.0001f) {
                pinchRatio = pinchDistance / palmSize;
            }

            // Evaluate pinch state with spatial hysteresis
            bool isPhysicallyPinching = false;
            if (!m_isLeftClicked && pinchRatio < m_clickThreshold) {
                isPhysicallyPinching = true;
            } else if (m_isLeftClicked && pinchRatio < (m_clickThreshold + 0.05f)) {
                isPhysicallyPinching = true;
            }

            // Evaluate Index finger extension
            float indexTipDist = std::sqrt(std::pow(rawIndexX - rawWristX, 2) + std::pow(rawIndexY - rawWristY, 2) + std::pow(rawIndexZ - rawWristZ, 2));
            float rawIndexPipX = data[6 * 3]; float rawIndexPipY = data[6 * 3 + 1]; float rawIndexPipZ = data[6 * 3 + 2];
            float indexPipDist = std::sqrt(std::pow(rawIndexPipX - rawWristX, 2) + std::pow(rawIndexPipY - rawWristY, 2) + std::pow(rawIndexPipZ - rawWristZ, 2));
            // Add a spatial margin (15% of palm size) to require a clearly straightened finger
            bool isIndexFingerUp = (indexTipDist > indexPipDist + (palmSize * 0.15f));

            // Evaluate Middle finger extension
            float midTipDist = std::sqrt(std::pow(rawMidTipX - rawWristX, 2) + std::pow(rawMidTipY - rawWristY, 2) + std::pow(rawMidTipZ - rawWristZ, 2));
            float midPipDist = std::sqrt(std::pow(rawMidPipX - rawWristX, 2) + std::pow(rawMidPipY - rawWristY, 2) + std::pow(rawMidPipZ - rawWristZ, 2));
            bool isMiddleFingerUp = (midTipDist > midPipDist + (palmSize * 0.15f));

            // Evaluate Ring finger extension
            float ringTipDist = std::sqrt(std::pow(rawRingTipX - rawWristX, 2) + std::pow(rawRingTipY - rawWristY, 2) + std::pow(rawRingTipZ - rawWristZ, 2));
            float rawRingPipX = data[14 * 3]; float rawRingPipY = data[14 * 3 + 1]; float rawRingPipZ = data[14 * 3 + 2];
            float ringPipDist = std::sqrt(std::pow(rawRingPipX - rawWristX, 2) + std::pow(rawRingPipY - rawWristY, 2) + std::pow(rawRingPipZ - rawWristZ, 2));
            bool isRingFingerUp = (ringTipDist > ringPipDist + (palmSize * 0.15f));

            // Evaluate Pinky finger extension
            float pinkyTipDist = std::sqrt(std::pow(rawPinkyTipX - rawWristX, 2) + std::pow(rawPinkyTipY - rawWristY, 2) + std::pow(rawPinkyTipZ - rawWristZ, 2));
            float rawPinkyPipX = data[18 * 3]; float rawPinkyPipY = data[18 * 3 + 1]; float rawPinkyPipZ = data[18 * 3 + 2];
            float pinkyPipDist = std::sqrt(std::pow(rawPinkyPipX - rawWristX, 2) + std::pow(rawPinkyPipY - rawWristY, 2) + std::pow(rawPinkyPipZ - rawWristZ, 2));
            bool isPinkyFingerUp = (pinkyTipDist > pinkyPipDist + (palmSize * 0.15f));

            // Evaluate Thumb folding (Tip vs Index finger MCP joint)
            float rawIndexMcpX = data[5 * 3];
            float rawIndexMcpY = data[5 * 3 + 1];
            float rawIndexMcpZ = data[5 * 3 + 2];
            
            float thumbToIndexDx = rawThumbX - rawIndexMcpX;
            float thumbToIndexDy = rawThumbY - rawIndexMcpY;
            float thumbToIndexDz = rawThumbZ - rawIndexMcpZ;
            
            float thumbToIndexDist = std::sqrt((thumbToIndexDx * thumbToIndexDx) + 
                                               (thumbToIndexDy * thumbToIndexDy) + 
                                               (thumbToIndexDz * thumbToIndexDz));
            
            // Thumb is folded if its tip is close to the base of the index finger
            bool isThumbFolded = (thumbToIndexDist < (palmSize * 0.40f));

            // Require all four main fingers to be clearly extended outward for pause
            bool isPalmOpen = isIndexFingerUp && isMiddleFingerUp && isRingFingerUp && isPinkyFingerUp && !isPhysicallyPinching;
            
            // Right-click gesture: Thumb is explicitly extended, Index is folded, others are folded (Thumbs-up shape)
            bool isRightClickGesture = !isThumbFolded && !isIndexFingerUp && !isMiddleFingerUp && !isRingFingerUp && !isPinkyFingerUp;
            
            // --- SYSTEM PAUSE TOGGLE ---
            
            // Require the user to hold the open palm gesture continuously
            if (isPalmOpen) {
                m_pauseFrames++;
            } else {
                // Instantly reset the holding counter if a finger drops or twitches
                m_pauseFrames = 0;
            }

            // Decrease the cooldown timer only when the hand is NOT fully open
            if (!isPalmOpen && m_pauseCooldown > 0) {
                m_pauseCooldown--;
            }

            // Toggle the pause state only if the gesture is held for 10 consecutive frames (approx 300ms)
            if (m_pauseFrames >= 10 && m_pauseCooldown == 0) {
                m_isPaused = !m_isPaused;
                
                // Require a 30-frame reset period after the hand closes
                m_pauseCooldown = 30; 
                
                // Reset the holding counter so it doesn't immediately trigger again
                m_pauseFrames = 0;
                
                emit isPausedChanged();
                
                if (m_isPaused) {
                    if (m_isLeftClicked) {
                        mouse->click(false);
                        m_isLeftClicked = false;
                    }
                    m_isScrolling = false;
                    m_scrollAccumulator = 0.0f;
                }
            }
            
            // --- PARALLEL EXECUTION PIPELINE ---
            
            // Execute mouse controls only if the system is active
            if (!m_isPaused) {
                
                // 1. SCROLL PIPELINE
                bool intendsToScroll = isMiddleFingerUp;

                if (intendsToScroll) {
                    m_scrollReleaseCounter = 0; 
                    if (!m_isScrolling) {
                        m_isScrolling = true;
                        m_scrollAnchorY = palmMark.y; 
                    }
                } else {
                    if (m_isScrolling) {
                        m_scrollReleaseCounter++;
                        if (m_scrollReleaseCounter >= 5) {
                            m_isScrolling = false;
                            m_scrollReleaseCounter = 0;
                            m_scrollAccumulator = 0.0f; 
                        }
                    }
                }

                if (m_isScrolling) {
                    float offset = palmMark.y - m_scrollAnchorY;
                    float deadzone = 0.04f;

                    if (std::abs(offset) > deadzone) {
                        float activeOffset = std::abs(offset) - deadzone;
                        int direction = (offset < 0) ? 1 : -1;
                        float currentSpeed = activeOffset * m_scrollSensitivity;
                        
                        m_scrollAccumulator += (currentSpeed * direction);
                        
                        if (std::abs(m_scrollAccumulator) >= 1.0f) {
                            int stepsToScroll = static_cast<int>(m_scrollAccumulator);
                            mouse->scroll(stepsToScroll);
                            m_scrollAccumulator -= stepsToScroll;
                        }
                    } else {
                        m_scrollAccumulator = 0.0f;
                    }
                }

                // 2. MOVEMENT PIPELINE
                float scaledX = ((palmMark.x - 0.5f) * m_movementScale) + 0.5f;
                float scaledY = ((palmMark.y - 0.5f) * m_movementScale) + 0.5f;

                if (m_isFirstFrame) {
                    m_smoothedX = scaledX;
                    m_smoothedY = scaledY;
                    m_isFirstFrame = false;
                } else {
                    m_smoothedX = (scaledX * m_smoothingFactor) + (m_smoothedX * (1.0f - m_smoothingFactor));
                    m_smoothedY = (scaledY * m_smoothingFactor) + (m_smoothedY * (1.0f - m_smoothingFactor));
                }

                mouse->move(m_smoothedX, m_smoothedY);

                // 3. CLICK AND DRAG PIPELINE
                if (isPhysicallyPinching) {
                    m_releaseFrameCounter = 0;
                    if (!m_isLeftClicked) {
                        mouse->click(true);
                        m_isLeftClicked = true;
                    }
                } else {
                    if (m_isLeftClicked) {
                        m_releaseFrameCounter++;
                        if (m_releaseFrameCounter >= 4) {
                            mouse->click(false);
                            m_isLeftClicked = false;
                            m_releaseFrameCounter = 0;
                        }
                    }
                }
                // 4. RIGHT CLICK PIPELINE
                if (isRightClickGesture) {
                    m_rightReleaseFrameCounter = 0;
                    if (!m_isRightClicked) {
                        mouse->rightClick(true);
                        m_isRightClicked = true;
                    }
                } else {
                    if (m_isRightClicked) {
                        m_rightReleaseFrameCounter++;
                        if (m_rightReleaseFrameCounter >= 4) {
                            mouse->rightClick(false);
                            m_isRightClicked = false;
                            m_rightReleaseFrameCounter = 0;
                        }
                    }
                }
            } // Konec exekučního bloku pro aktivní stav
        }
    } else {
        std::cout << "Valid 2D landmark tensor not found." << std::endl;
    }
}