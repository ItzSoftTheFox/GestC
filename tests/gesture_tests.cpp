#include "gestures/gesture_engine.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace handmouse;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
Hand makeHand(Pose pose) {
    Hand h; h.confidence = 0.95;
    h.points[0] = {0.5, 0.8, 0};
    h.points[4] = {0.24, 0.66, 0};
    for (int finger = 0; finger < 4; ++finger) {
        const int base = 5 + finger * 4;
        const double x = 0.38 + finger * 0.08;
        const bool up = pose != Pose::Scroll && (pose != Pose::Pause || finger == 1);
        h.points[base] = {x, 0.58, 0};
        h.points[base + 1] = {x, 0.43, 0};
        h.points[base + 2] = {x, up ? 0.33 : 0.55, 0};
        h.points[base + 3] = {x, up ? 0.25 : 0.69, 0};
    }
    if (pose == Pose::LeftDrag) h.points[4] = h.points[8];
    if (pose == Pose::SuperDrag) h.points[4] = h.points[20];
    if (pose == Pose::MiddleClick) h.points[4] = h.points[16];
    return h;
}
Hand shifted(Hand h, double x, double y) { for (auto& p : h.points) { p.x += x; p.y += y; } return h; }
int main() {
    try {
        Settings settings; settings.smoothing = 0;
        for (Pose p : {Pose::Move, Pose::Scroll, Pose::LeftDrag, Pose::SuperDrag, Pose::MiddleClick, Pose::Pause})
            require(GestureEngine::classify(makeHand(p), settings.pinchThreshold) == p, "gesture classification");
        GestureEngine engine;
        auto open = makeHand(Pose::Move);
        engine.update(open, 0, settings);
        auto result = engine.update(open, .08, settings);
        require(result.dx == 0 && result.dy == 0, "first acquired hand must not move cursor");
        result = engine.update(shifted(open, .1, 0), .12, settings);
        require(std::abs(result.dx - 180) < .01, "relative movement");
        auto pinch = makeHand(Pose::LeftDrag);
        require(!engine.update(pinch, .16, settings).left, "transient pinch must not click");
        require(engine.update(pinch, .24, settings).left, "pinch holds left button");
        result = engine.update(std::nullopt, .28, settings);
        require(!result.left && !result.super, "hand loss releases buttons");
        engine.update(pinch, .32, settings);
        result = engine.update(pinch, .40, settings);
        require(result.left && result.dx == 0, "reacquisition starts without jump");
        result = engine.update(pinch, 1.0, settings);
        require(!result.left, "stale frame gap releases drag");
        auto super = makeHand(Pose::SuperDrag);
        engine.reset(); engine.update(super, 0, settings);
        result = engine.update(super, .08, settings);
        require(result.left && result.super, "Super drag holds modifier and left button");
        engine.setPaused(true);
        result = engine.update(super, .12, settings);
        require(!result.left && !result.super && result.paused, "manual pause releases all input");
        auto pause = makeHand(Pose::Pause);
        engine.update(pause, .16, settings);
        for (int i = 1; i <= 20; ++i) result = engine.update(pause, .16 + i * .04, settings);
        require(!result.paused, "middle finger resumes after hold");
        for (int i = 21; i <= 45; ++i) result = engine.update(pause, .16 + i * .04, settings);
        require(!result.paused, "held pause gesture toggles only once");
        engine.reset();
        auto middle = makeHand(Pose::MiddleClick);
        engine.update(middle, 0, settings);
        require(engine.update(middle, .08, settings).middleClick, "ring pinch clicks middle");
        require(!engine.update(middle, .12, settings).middleClick, "holding ring pinch must not repeat click");
        engine.update(open, .16, settings);
        engine.update(middle, .20, settings);
        require(engine.update(middle, .28, settings).middleClick, "release rearms middle click");
        engine.reset(); auto fist = makeHand(Pose::Scroll);
        engine.update(fist, 0, settings); engine.update(fist, .08, settings);
        result = engine.update(fist, .12, settings);
        require(result.scroll == 0, "scroll anchor deadzone");
        int total = 0;
        for (int i = 1; i <= 10; ++i) { result = engine.update(shifted(fist, 0, -.1), .12 + .04 * i, settings); total += result.scroll; require(result.dx == 0 && result.dy == 0, "scroll does not move pointer"); }
        require(total > 0, "fist above anchor scrolls up");
        auto invalid = open; invalid.points[8].x = std::numeric_limits<double>::quiet_NaN();
        require(GestureEngine::classify(invalid, .3) == Pose::None, "reject nonfinite landmarks");
        invalid = open; invalid.confidence = .1;
        require(!engine.update(invalid, 1, settings).left, "reject low confidence");
        // Time-based smoothing should produce the same path at different frame rates.
        const auto motion = [&](int hz) {
            GestureEngine e; Settings s; double sum = 0;
            e.update(open, 0, s); e.update(open, .08, s);
            for (int i = 1; i <= hz; ++i) sum += e.update(shifted(open, .1, 0), .08 + double(i) / hz, s).dx;
            return sum;
        };
        require(std::abs(motion(30) - motion(60)) < 0.1, "smoothing independent of frame rate");
        std::cout << "Gesture, timing, pause, loss and movement tests passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
