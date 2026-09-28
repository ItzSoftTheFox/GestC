#include "tracking/hand_tracker.hpp"
#include <opencv2/imgcodecs.hpp>
#include <iostream>
#include <cmath>
int main(int argc, char** argv) {
    if (argc < 2) return 2;
    try {
        HandTracker tracker; tracker.load(argv[1]);
        for (const cv::Size size : {cv::Size(640, 480), cv::Size(480, 640), cv::Size(640, 640)}) {
            if (tracker.track(cv::Mat::zeros(size, CV_8UC3), .65)) {
                std::cerr << "False detection on blank frame\n"; return 1;
            }
        }
        if (argc > 2) {
            auto frame = cv::imread(argv[2]);
            if (frame.empty()) return 2;
            auto hand = tracker.track(frame, .65);
            if (!hand) { std::cerr << "No hand in fixture\n"; return 1; }
            std::cout << "Detected hand, confidence " << hand->confidence << '\n';
            for (int i = 0; i < 30; ++i) {
                const auto tracked = tracker.track(frame, .65);
                if (!tracked || tracked->confidence < .8) { std::cerr << "Tracking lost confidence on repeated fixture\n"; return 1; }
                if (std::hypot(tracked->points[9].x - hand->points[9].x, tracked->points[9].y - hand->points[9].y) > .08) {
                    std::cerr << "Tracking drifts on a static hand\n"; return 1;
                }
            }
            if (tracker.track(cv::Mat::zeros(frame.size(), CV_8UC3), .65)) { std::cerr << "Hand loss was not detected\n"; return 1; }
            if (!tracker.track(frame, .65)) { std::cerr << "Hand reacquisition failed\n"; return 1; }
            cv::Mat mirrored, rotated;
            cv::flip(frame, mirrored, 1); cv::rotate(frame, rotated, cv::ROTATE_90_CLOCKWISE);
            for (const auto& variant : {mirrored, rotated}) {
                tracker.reset();
                if (!tracker.track(variant, .65)) { std::cerr << "Mirrored/rotated hand not detected\n"; return 1; }
            }
            HandTracker::draw(frame, *hand);
            cv::imwrite("/tmp/handmouse-model-check.jpg", frame);
        }
        std::cout << "Models loaded; named tensor shapes, inference and blank frames verified.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
