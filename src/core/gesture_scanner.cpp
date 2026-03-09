#include "gesture_scanner.hpp"

GestureScanner::GestureScanner(QObject *parent) : QObject(parent), 
m_isScanning(false), m_offsetX(0.0f), m_offsetY(0.0f), m_cropMultiplier(2.0f), m_movementScale(1.5f), 
m_smoothingFactor(0.2f), m_clickThreshold(0.05f), m_isFirstFrame(true), m_isLeftClicked(false),
m_releaseFrameCounter(0), m_isScrolling(false), m_scrollAnchorY(0.0f), m_scrollSensitivity(15.0f) {
    
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

        // Print the highest probability and its index to the terminal
        std::cout << "Best Hand Score: " << maxScore << " at anchor index: " << bestIndex << std::endl;
        
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
                    // Isolate the hand without warping the aspect ratio
                    cv::Mat handCrop = frame(squareBox);
                    
                    // Process the clean, shifted crop in the landmark network
                    processLandmarks(handCrop, squareBox, frame.cols, frame.rows);
                }
            }
        }
    }
}

void GestureScanner::drawLandmarks(cv::Mat& frame) {
    for (const auto& lm : currentLandmarks) {
        // Calculate the exact pixel position on your real camera frame.
        // Landmarks are stored as normalized coordinates (0.0 - 1.0)
        int px = static_cast<int>(lm.x * frame.cols);
        int py = static_cast<int>(lm.y * frame.rows);
        
        // Ensure point is within frame bounds before drawing
        if (px >= 0 && px < frame.cols && py >= 0 && py < frame.rows) {
            // Draw a green circle at the calculated position.
            cv::circle(frame, cv::Point(px, py), 8, cv::Scalar(0, 255, 0), cv::FILLED);
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

        // 3. Extract coordinates for cursor movement (Middle finger base)
        float rawPalmX = data[9 * 3];
        float rawPalmY = data[9 * 3 + 1];
        float rawPalmZ = data[9 * 3 + 2];
            
        // 4. Extract coordinates for gesture recognition
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
            
        // 5. Normalize palm coordinates for screen movement
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

        // Store the palm coordinate for QML rendering
        HandLandmark palmMark;
        palmMark.x = globalX / static_cast<float>(frameWidth);
        palmMark.y = globalY / static_cast<float>(frameHeight);
        palmMark.z = 0.0f;
        currentLandmarks.push_back(palmMark);

        // 6. Execute system actions if the virtual mouse is ready
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

            // Calculate exact 3D distances from the wrist for the middle finger
            float tipDx = rawMidTipX - rawWristX;
            float tipDy = rawMidTipY - rawWristY;
            float tipDz = rawMidTipZ - rawWristZ;
            float distToTip = std::sqrt((tipDx * tipDx) + (tipDy * tipDy) + (tipDz * tipDz));

            float pipDx = rawMidPipX - rawWristX;
            float pipDy = rawMidPipY - rawWristY;
            float pipDz = rawMidPipZ - rawWristZ;
            float distToPip = std::sqrt((pipDx * pipDx) + (pipDy * pipDy) + (pipDz * pipDz));

            // The middle finger is extended if the tip is further from the wrist than the PIP joint
            bool isMiddleFingerUp = (distToTip > distToPip);

            // --- STATE MACHINE EXECUTION ---
            
            // 1. EVALUATE SCROLL STATE WITH DEBOUNCE
            bool intendsToScroll = (isMiddleFingerUp && !isPhysicallyPinching && !m_isLeftClicked);

            if (intendsToScroll) {
                // Reset the drop counter immediately when network sees the gesture
                m_scrollReleaseCounter = 0; 
                
                if (!m_isScrolling) {
                    m_isScrolling = true;
                    // FIX: Use the global normalized camera coordinate, NOT the local crop coordinate
                    m_scrollAnchorY = palmMark.y; 
                    std::cout << "JOYSTICK SCROLL ACTIVATED." << std::endl;
                }
            } else {
                if (m_isScrolling) {
                    // Increment buffer instead of exiting immediately
                    m_scrollReleaseCounter++;
                    
                    // Exit scroll mode only after 5 consecutive lost frames (approx 160ms)
                    if (m_scrollReleaseCounter >= 5) {
                        m_isScrolling = false;
                        m_scrollReleaseCounter = 0;
                        m_scrollAccumulator = 0.0f; // Safely reset the accumulator
                        std::cout << "JOYSTICK SCROLL DEACTIVATED." << std::endl;
                    }
                }
            }

            // 2. EXECUTE ACTIONS BASED ON FILTERED STATE
            if (m_isScrolling) {
                // Calculate distance using the global camera frame coordinate
                float offset = palmMark.y - m_scrollAnchorY;
                
                // Define a deadzone (0.04f = exactly 4% of the total vertical camera view)
                float deadzone = 0.04f;

                if (std::abs(offset) > deadzone) {
                    // Extract the active movement distance beyond the deadzone boundary
                    float activeOffset = std::abs(offset) - deadzone;
                    
                    // Determine the scroll direction
                    // A negative offset means the hand is physically above the anchor (scroll UP)
                    int direction = (offset < 0) ? 1 : -1;
                    
                    // Calculate the current scroll speed based on absolute distance and UI sensitivity
                    float currentSpeed = activeOffset * m_scrollSensitivity;
                    
                    // Add the calculated fractional speed to the system accumulator
                    m_scrollAccumulator += (currentSpeed * direction);
                    
                    // Send the hardware signal only after accumulating at least one full integer step
                    if (std::abs(m_scrollAccumulator) >= 1.0f) {
                        int stepsToScroll = static_cast<int>(m_scrollAccumulator);
                        mouse->scroll(stepsToScroll);
                        m_scrollAccumulator -= stepsToScroll;
                    }
                } else {
                    // Instantly flush the accumulator when the hand returns to the deadzone
                    m_scrollAccumulator = 0.0f;
                }
            } else {
                // --- NORMAL CURSOR MOVEMENT ---
                // Scale movement outward from the center
                float scaledX = ((palmMark.x - 0.5f) * m_movementScale) + 0.5f;
                float scaledY = ((palmMark.y - 0.5f) * m_movementScale) + 0.5f;

                // Apply Exponential Moving Average (EMA) for cursor stability
                if (m_isFirstFrame) {
                    m_smoothedX = scaledX;
                    m_smoothedY = scaledY;
                    m_isFirstFrame = false;
                } else {
                    m_smoothedX = (scaledX * m_smoothingFactor) + (m_smoothedX * (1.0f - m_smoothingFactor));
                    m_smoothedY = (scaledY * m_smoothingFactor) + (m_smoothedY * (1.0f - m_smoothingFactor));
                }

                // Move the mouse only when not scrolling
                mouse->move(m_smoothedX, m_smoothedY);

                // --- NORMAL CLICK EXECUTION ---
                if (isPhysicallyPinching) {
                    m_releaseFrameCounter = 0;
                    if (!m_isLeftClicked) {
                        mouse->click(true);
                        m_isLeftClicked = true;
                        std::cout << "DRAG STARTED." << std::endl;
                    }
                } else {
                    if (m_isLeftClicked) {
                        m_releaseFrameCounter++;
                        if (m_releaseFrameCounter >= 4) {
                            mouse->click(false);
                            m_isLeftClicked = false;
                            m_releaseFrameCounter = 0;
                            std::cout << "DRAG RELEASED." << std::endl;
                        }
                    }
                }
            }
        }
    } else {
        std::cout << "Valid 2D landmark tensor not found." << std::endl;
    }
}