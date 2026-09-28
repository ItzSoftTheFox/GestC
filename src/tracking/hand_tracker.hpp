#pragma once
#include "gestures/gesture_engine.hpp"
#include <opencv2/core.hpp>
#include <opencv2/dnn.hpp>
#include <optional>
#include <string>

class HandTracker {
public:
    void load(const std::string& modelDirectory);
    std::optional<handmouse::Hand> track(const cv::Mat& frame, double confidence);
    void reset() { roi_.reset(); }
    static void draw(cv::Mat& frame, const handmouse::Hand& hand);
private:
    struct Region { cv::Point2f center; float size; float angle; };
    std::optional<Region> detectPalm(const cv::Mat& frame);
    std::optional<handmouse::Hand> landmarks(const cv::Mat& frame, const Region& region, double confidence);
    Region regionFromHand(const handmouse::Hand& hand, const cv::Size& size) const;
    cv::dnn::Net palmNet_, landmarkNet_;
    std::vector<cv::Point2f> anchors_;
    std::optional<Region> roi_;
};
