#include "hand_tracker.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace {
constexpr int palmSize = 192;
constexpr int landmarkSize = 224;
constexpr float pi = 3.14159265358979323846f;
cv::dnn::Net loadModel(const std::string& path) {
    // OpenCV 5's new engine currently loses outputs of these bundled TFLite models.
#if CV_VERSION_MAJOR >= 5
    auto net = cv::dnn::readNetFromTFLite(path, cv::dnn::ENGINE_CLASSIC);
#else
    auto net = cv::dnn::readNetFromTFLite(path);
#endif
    net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
    net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    return net;
}
void verify(const cv::Mat& mat, size_t count, const char* name) {
    if (mat.total() != count || mat.type() != CV_32F || !mat.isContinuous() || !cv::checkRange(mat))
        throw std::runtime_error(std::string("Neočekávaný výstup modelu: ") + name);
}
}
void HandTracker::load(const std::string& directory) {
    cv::setNumThreads(2);
    palmNet_ = loadModel(directory + "/hand_detector.tflite");
    landmarkNet_ = loadModel(directory + "/hand_landmarks_detector.tflite");
    anchors_.clear();
    for (const auto& grid : {std::pair<int,int>{24, 2}, {12, 6}})
        for (int y = 0; y < grid.first; ++y)
            for (int x = 0; x < grid.first; ++x)
                for (int i = 0; i < grid.second; ++i)
                    anchors_.emplace_back((x + 0.5f) / grid.first, (y + 0.5f) / grid.first);
    // Warm up and validate all named outputs, including presence (not handedness).
    std::vector<cv::Mat> outputs;
    palmNet_.setInput(cv::dnn::blobFromImage(cv::Mat::zeros(palmSize, palmSize, CV_8UC3), 1.0 / 255));
    palmNet_.forward(outputs, std::vector<cv::String>{"Identity", "Identity_1"});
    if (outputs.size() != 2) throw std::runtime_error("Chybí výstupy detektoru dlaně.");
    verify(outputs[0], 2016 * 18, "palm boxes"); verify(outputs[1], 2016, "palm scores");
    landmarkNet_.setInput(cv::dnn::blobFromImage(cv::Mat::zeros(landmarkSize, landmarkSize, CV_8UC3), 1.0 / 255));
    landmarkNet_.forward(outputs, std::vector<cv::String>{"Identity", "Identity_1"});
    if (outputs.size() != 2) throw std::runtime_error("Chybí výstupy modelu ruky.");
    verify(outputs[0], 63, "landmarks"); verify(outputs[1], 1, "presence");
    reset();
}
std::optional<HandTracker::Region> HandTracker::detectPalm(const cv::Mat& frame) {
    // Letterbox preserves hand geometry on non-square camera frames.
    const float scale = static_cast<float>(palmSize) / std::max(frame.cols, frame.rows);
    cv::Mat resized, input = cv::Mat::zeros(palmSize, palmSize, CV_8UC3);
    cv::resize(frame, resized, cv::Size(), scale, scale);
    const int padX = (palmSize - resized.cols) / 2, padY = (palmSize - resized.rows) / 2;
    resized.copyTo(input(cv::Rect(padX, padY, resized.cols, resized.rows)));
    palmNet_.setInput(cv::dnn::blobFromImage(input, 1.0 / 255, {}, {}, true));
    std::vector<cv::Mat> outputs;
    palmNet_.forward(outputs, std::vector<cv::String>{"Identity", "Identity_1"});
    verify(outputs.at(0), 2016 * 18, "palm boxes"); verify(outputs.at(1), 2016, "palm scores");
    const float* scores = outputs[1].ptr<float>();
    const int best = static_cast<int>(std::max_element(scores, scores + anchors_.size()) - scores);
    if (scores[best] < 0.62f) return std::nullopt; // sigmoid(0.62) ~= 0.65
    const float* box = outputs[0].ptr<float>() + best * 18;
    const auto decode = [&](int offset) {
        return cv::Point2f((box[offset] + anchors_[best].x * palmSize - padX) / scale,
                           (box[offset + 1] + anchors_[best].y * palmSize - padY) / scale);
    };
    const cv::Point2f wrist = decode(4), middleBase = decode(8);
    const float angle = std::atan2(middleBase.y - wrist.y, middleBase.x - wrist.x) + pi / 2;
    const float size = std::max(box[2], box[3]) / scale;
    if (size < 8 || size > std::max(frame.cols, frame.rows) * 2) return std::nullopt;
    const cv::Point2f up(std::sin(angle), -std::cos(angle));
    return Region{decode(0) + up * (size * 0.5f), size * 2.6f, angle};
}
std::optional<handmouse::Hand> HandTracker::landmarks(const cv::Mat& frame, const Region& roi, double confidence) {
    const cv::Point2f right(std::cos(roi.angle), std::sin(roi.angle));
    const cv::Point2f down(-std::sin(roi.angle), std::cos(roi.angle));
    const cv::Point2f origin = roi.center - (right + down) * (roi.size / 2);
    const double factor = landmarkSize / roi.size;
    const cv::Matx23d transform(factor * right.x, factor * right.y, -factor * right.dot(origin),
                               factor * down.x, factor * down.y, -factor * down.dot(origin));
    cv::Mat crop;
    cv::warpAffine(frame, crop, transform, cv::Size(landmarkSize, landmarkSize), cv::INTER_LINEAR, cv::BORDER_CONSTANT);
    landmarkNet_.setInput(cv::dnn::blobFromImage(crop, 1.0 / 255, {}, {}, true));
    std::vector<cv::Mat> outputs;
    landmarkNet_.forward(outputs, std::vector<cv::String>{"Identity", "Identity_1"});
    verify(outputs.at(0), 63, "landmarks"); verify(outputs.at(1), 1, "presence");
    const float presence = outputs[1].ptr<float>()[0];
    if (presence < confidence) return std::nullopt;
    handmouse::Hand hand;
    hand.confidence = presence;
    const float* points = outputs[0].ptr<float>();
    // Both axes use frame width, keeping distances and cursor speed isotropic.
    for (int i = 0; i < 21; ++i) {
        const cv::Point2f pixel = origin + right * (points[i * 3] * roi.size / landmarkSize)
                                       + down * (points[i * 3 + 1] * roi.size / landmarkSize);
        hand.points[i] = {pixel.x / frame.cols, pixel.y / frame.cols, points[i * 3 + 2] * roi.size / landmarkSize / frame.cols};
    }
    return hand;
}
HandTracker::Region HandTracker::regionFromHand(const handmouse::Hand& hand, const cv::Size& size) const {
    const auto& wrist = hand.points[0]; const auto& middle = hand.points[9];
    const float angle = static_cast<float>(std::atan2(middle.y - wrist.y, middle.x - wrist.x)) + pi / 2;
    const cv::Point2f right(std::cos(angle), std::sin(angle)), down(-std::sin(angle), std::cos(angle));
    float minX = 1e6, minY = 1e6, maxX = -1e6, maxY = -1e6;
    for (const auto& point : hand.points) {
        const cv::Point2f pixel(point.x * size.width, point.y * size.width);
        const float x = pixel.dot(right), y = pixel.dot(down);
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    }
    const cv::Point2f center = right * ((minX + maxX) / 2) + down * ((minY + maxY) / 2 - (maxY - minY) * 0.05f);
    return {center, std::clamp(std::max(maxX - minX, maxY - minY) * 1.5f, 32.f, static_cast<float>(std::max(size.width, size.height)) * 2), angle};
}
std::optional<handmouse::Hand> HandTracker::track(const cv::Mat& frame, double confidence) {
    if (frame.empty()) { reset(); return std::nullopt; }
    std::optional<handmouse::Hand> hand;
    if (roi_) hand = landmarks(frame, *roi_, confidence);
    if (!hand) {
        roi_ = detectPalm(frame);
        if (roi_) hand = landmarks(frame, *roi_, confidence);
    }
    if (hand) roi_ = regionFromHand(*hand, frame.size());
    else reset();
    return hand;
}
void HandTracker::draw(cv::Mat& frame, const handmouse::Hand& hand) {
    const std::array<std::pair<int,int>, 21> edges{{{0,1},{1,2},{2,3},{3,4},{0,5},{5,6},{6,7},{7,8},{5,9},{9,10},{10,11},{11,12},{9,13},{13,14},{14,15},{15,16},{13,17},{0,17},{17,18},{18,19},{19,20}}};
    const auto pixel = [&](int i) { return cv::Point(static_cast<int>(hand.points[i].x * frame.cols), static_cast<int>(hand.points[i].y * frame.cols)); };
    for (const auto& edge : edges) cv::line(frame, pixel(edge.first), pixel(edge.second), cv::Scalar(210, 220, 100), 2, cv::LINE_AA);
    for (int i = 0; i < 21; ++i) cv::circle(frame, pixel(i), i % 4 == 0 ? 5 : 3, cv::Scalar(245, 250, 235), -1, cv::LINE_AA);
}
