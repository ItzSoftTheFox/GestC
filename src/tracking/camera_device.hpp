#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct CameraMode {
    int width = 0, height = 0;
    double fps = 0;
    uint32_t format = 0;
};
struct CameraCapabilities {
    bool capture = false;
    std::vector<CameraMode> modes;
    std::string error;
};
CameraCapabilities cameraCapabilities(int index);
std::optional<CameraMode> chooseCameraMode(const std::vector<CameraMode>& modes, int requestedFps, int width = 0, int height = 0);
std::string cameraFormatName(uint32_t format);

// Keep automatic exposure, but prevent it from extending the frame interval.
// Restore the camera's previous control value when this capture session ends.
class CameraExposure {
public:
    explicit CameraExposure(int index);
    ~CameraExposure();
    CameraExposure(const CameraExposure&) = delete;
    CameraExposure& operator=(const CameraExposure&) = delete;
    bool fixedFrameRate() const { return fixed_; }
private:
    int fd_ = -1, original_ = 0;
    bool changed_ = false, fixed_ = false;
};
