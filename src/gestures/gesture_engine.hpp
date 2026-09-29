#pragma once
#include <array>
#include <optional>
#include <string>

namespace handmouse {
struct Point { double x = 0; double y = 0; double z = 0; };
struct Hand { std::array<Point, 21> points{}; double confidence = 0; };
struct Settings {
    double speed = 1.0;
    double smoothing = 0.55;
    double scrollSpeed = 12.0;
    double pinchThreshold = 0.30;
    double confidence = 0.65;
    int camera = 0;
    int cameraFps = 30;
    int cameraWidth = 0, cameraHeight = 0; // Zero selects automatic resolution.
    bool mirror = true;
    bool skeleton = true;
    bool previewOnly = true;
};
enum class Pose { None, Move, Scroll, LeftDrag, SuperDrag, MiddleClick, Pause };
struct Command {
    double dx = 0, dy = 0;
    int scroll = 0;
    bool left = false, super = false, middleClick = false;
    bool paused = false;
    bool safetyBlocked = false;
    Pose pose = Pose::None;
};

class GestureEngine {
public:
    Command update(const std::optional<Hand>& hand, double seconds, const Settings& settings);
    void reset();
    void suspend();
    void setPaused(bool paused);
    bool paused() const { return paused_; }
    static Pose classify(const Hand& hand, double pinchThreshold);
    static const char* poseName(Pose pose);
private:
    bool paused_ = false;
    bool safetyBlocked_ = false, scrollSession_ = false;
    double openSince_ = -1;
    std::optional<Point> lastPalm_, openPalm_;
    double lastPalmSize_ = 0;
    double scrollPosition_ = 0;
    bool pauseLatched_ = false;
    bool middleLatched_ = false;
    Pose candidate_ = Pose::None;
    Pose active_ = Pose::None;
    double candidateSince_ = 0;
    double lastTime_ = -1;
    double pauseReleaseSince_ = -1;
    std::optional<Point> previous_;
    Point filtered_{};
    double scrollAnchor_ = 0;
    double scrollRemainder_ = 0;
};
}
