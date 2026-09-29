#include "gesture_engine.hpp"
#include <algorithm>
#include <cmath>

namespace handmouse {
namespace {
double distance(const Point& a, const Point& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}
bool extended(const Hand& h, int tip) {
    const auto& wrist = h.points[0];
    const auto& pip = h.points[tip - 2];
    const auto& mcp = h.points[tip - 3];
    const auto& end = h.points[tip];
    const double ax = mcp.x - pip.x, ay = mcp.y - pip.y;
    const double bx = end.x - pip.x, by = end.y - pip.y;
    const double length = std::hypot(ax, ay) * std::hypot(bx, by);
    return length > 1e-8 && (ax * bx + ay * by) / length < -0.65
        && distance(end, wrist) > distance(pip, wrist) * 1.12;
}
Point palm(const Hand& h) {
    return {(h.points[0].x + h.points[9].x) / 2,
            (h.points[0].y + h.points[9].y) / 2, 0};
}
}
Pose GestureEngine::classify(const Hand& h, double threshold) {
    for (const auto& p : h.points)
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) return Pose::None;
    const double size = distance(h.points[0], h.points[9]);
    if (size < 0.015) return Pose::None;
    const bool index = extended(h, 8), middle = extended(h, 12);
    const bool ring = extended(h, 16), pinky = extended(h, 20);
    if (middle && !index && !ring && !pinky) return Pose::Pause;
    // A closed fist wins over incidental thumb contact inside the fist.
    if (!index && !middle && !ring && !pinky) return Pose::Scroll;
    // Pick the closest fingertip so adjacent pinches cannot trigger two actions.
    int closest = 8;
    for (int tip : {16, 20})
        if (distance(h.points[tip], h.points[4]) < distance(h.points[closest], h.points[4])) closest = tip;
    if (distance(h.points[closest], h.points[4]) / size < threshold) {
        if (closest == 20) return Pose::SuperDrag;
        if (closest == 16) return Pose::MiddleClick;
        return Pose::LeftDrag;
    }
    if (index && middle && ring && pinky) return Pose::Move;
    return Pose::None;
}
void GestureEngine::reset() {
    safetyBlocked_ = scrollSession_ = false;
    openSince_ = -1; openPalm_.reset(); lastPalm_.reset(); lastPalmSize_ = 0;
    candidate_ = active_ = Pose::None;
    lastTime_ = -1;
    previous_.reset();
    scrollRemainder_ = 0;
    middleLatched_ = false;
    pauseLatched_ = false;
    pauseReleaseSince_ = -1;
}
void GestureEngine::suspend() {
    reset();
    safetyBlocked_ = true;
}
void GestureEngine::setPaused(bool paused) {
    reset();
    paused_ = paused;
}
Command GestureEngine::update(const std::optional<Hand>& hand, double now, const Settings& settings) {
    Command out;
    out.paused = paused_;
    if (!hand || !std::isfinite(hand->confidence) || hand->confidence < settings.confidence || !std::isfinite(now)) {
        if (lastTime_ >= 0 || safetyBlocked_) suspend();
        out.safetyBlocked = safetyBlocked_;
        return out;
    }
    const double dt = lastTime_ < 0 ? 1.0 / 30 : std::clamp(now - lastTime_, 0.001, 0.10);
    // A gap must never turn reacquisition into a cursor jump or held key.
    if (lastTime_ >= 0 && (now - lastTime_ > 0.25 || now <= lastTime_)) suspend();
    lastTime_ = now;
    double threshold = settings.pinchThreshold;
    if (active_ == Pose::LeftDrag || active_ == Pose::SuperDrag || active_ == Pose::MiddleClick) threshold += 0.07;
    const Pose pose = classify(*hand, threshold);
    out.pose = pose;
    const Point position = palm(*hand);
    const double palmSize = distance(hand->points[0], hand->points[9]);
    // Do not turn a confident but discontinuous landmark estimate into input.
    const double jumpLimit = std::max(0.08, std::min(0.22, lastPalmSize_ * 0.9)) + 0.6 * dt;
    if (lastPalm_ && (distance(position, *lastPalm_) > jumpLimit ||
        palmSize < lastPalmSize_ * 0.6 || palmSize > lastPalmSize_ * 1.65)) suspend();
    if (pose == Pose::None || (scrollSession_ && pose != Pose::Scroll)) suspend();
    if (pose != Pose::None) { lastPalm_ = position; lastPalmSize_ = palmSize; }
    if (safetyBlocked_) {
        out.safetyBlocked = true;
        if (pose != Pose::Move) { openSince_ = -1; openPalm_.reset(); }
        else {
            if (openSince_ < 0 || !openPalm_ || distance(position, *openPalm_) > std::max(0.02, palmSize * 0.15)) {
                openSince_ = now; openPalm_ = position;
            }
            if (now - openSince_ >= 0.2) {
                reset();
                lastTime_ = now; lastPalm_ = position; lastPalmSize_ = palmSize;
                active_ = candidate_ = Pose::Move; candidateSince_ = now;
                filtered_ = position; previous_ = position;
                out.safetyBlocked = false;
            }
        }
        return out; // Rearming itself never moves, clicks or scrolls.
    }
    if (pose == Pose::Scroll) scrollSession_ = true;
    if (pose != candidate_) {
        candidate_ = pose;
        candidateSince_ = now;
    }
    if (pose == Pose::Pause) {
        pauseReleaseSince_ = -1;
        if (!pauseLatched_ && now - candidateSince_ >= 0.5) {
            paused_ = !paused_;
            pauseLatched_ = true;
        }
    } else {
        if (pauseReleaseSince_ < 0) pauseReleaseSince_ = now;
        if (now - pauseReleaseSince_ > 0.25) pauseLatched_ = false;
    }
    out.paused = paused_;
    // Pause and unrecognized transitions release input immediately.
    if (paused_ || pose == Pose::Pause || pose == Pose::None) {
        active_ = Pose::None;
        previous_.reset();
        scrollRemainder_ = 0;
        if (pose != Pose::MiddleClick) middleLatched_ = false;
        return out;
    }
    if (pose != Pose::MiddleClick) middleLatched_ = false;
    if (pose != active_) {
        previous_.reset();
        scrollRemainder_ = 0;
        active_ = Pose::None;
        if (now - candidateSince_ < 0.065) return out;
        active_ = pose;
        scrollAnchor_ = scrollPosition_ = position.y;
    }
    if (pose == Pose::Scroll) {
        scrollPosition_ += (1 - std::exp(-dt / 0.08)) * (position.y - scrollPosition_);
        const double offset = scrollAnchor_ - scrollPosition_;
        const double outsideDeadzone = std::max(0.0, std::abs(offset) - 0.025);
        const double rate = std::clamp(std::copysign(outsideDeadzone, offset) * settings.scrollSpeed * 15, -12.0, 12.0);
        scrollRemainder_ += rate * dt;
        const int wholeSteps = static_cast<int>(scrollRemainder_);
        out.scroll = std::clamp(wholeSteps, -1, 1);
        scrollRemainder_ -= wholeSteps; // Discard bursts; never replay accumulated input.
        return out;
    }
    if (pose == Pose::MiddleClick) {
        out.middleClick = !middleLatched_;
        middleLatched_ = true;
        return out;
    }
    out.left = pose == Pose::LeftDrag || pose == Pose::SuperDrag;
    out.super = pose == Pose::SuperDrag;
    if (!previous_) {
        filtered_ = position;
        previous_ = filtered_;
        return out;
    }
    const double tau = settings.smoothing * settings.smoothing * 0.22;
    const double alpha = tau < 0.001 ? 1 : 1 - std::exp(-dt / tau);
    filtered_.x += alpha * (position.x - filtered_.x);
    filtered_.y += alpha * (position.y - filtered_.y);
    out.dx = (filtered_.x - previous_->x) * 1800 * settings.speed;
    out.dy = (filtered_.y - previous_->y) * 1800 * settings.speed;
    previous_ = filtered_;
    return out;
}
const char* GestureEngine::poseName(Pose pose) {
    switch (pose) {
    case Pose::Move: return "Pohyb kurzoru";
    case Pose::Scroll: return "Scrollování";
    case Pose::LeftDrag: return "Klik / tažení";
    case Pose::SuperDrag: return "Super + tažení";
    case Pose::MiddleClick: return "Klik kolečkem";
    case Pose::Pause: return "Přepnutí pauzy";
    default: return "Ukaž otevřenou ruku";
    }
}
}
