#include "tracking/camera_device.hpp"
#include <linux/videodev2.h>
#include <iostream>
#include <stdexcept>
#include <limits>
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        std::vector<CameraMode> modes{{1280,720,10,V4L2_PIX_FMT_YUYV}, {640,480,30,V4L2_PIX_FMT_YUYV},
            {640,480,30,V4L2_PIX_FMT_MJPEG}, {320,240,60,V4L2_PIX_FMT_MJPEG}};
        auto mode = chooseCameraMode(modes, 30);
        require(mode && mode->width == 640 && mode->fps == 30 && mode->format == V4L2_PIX_FMT_MJPEG, "prefer 640x480 MJPEG at requested rate");
        mode = chooseCameraMode(modes, 60);
        require(mode && mode->width == 320 && mode->fps == 60, "60 FPS can require a lower resolution");
        modes.push_back({1280,720,30,V4L2_PIX_FMT_MJPEG});
        mode = chooseCameraMode(modes, 30, 1280, 720);
        require(mode && mode->width == 1280 && mode->height == 720 && mode->fps == 30 && mode->format == V4L2_PIX_FMT_MJPEG, "720p selects MJPEG at 30, not 10fps YUYV");
        mode = chooseCameraMode(modes, 60, 1280, 720);
        require(mode && mode->width == 1280 && mode->fps == 30, "explicit resolution takes precedence over FPS");
        mode = chooseCameraMode(modes, 30, 1920, 1080);
        require(mode && mode->fps == 30, "missing resolution uses an available mode");
        modes.pop_back();
        modes.pop_back(); mode = chooseCameraMode(modes, 60);
        require(mode && mode->fps == 30, "unsupported 60 falls back to actual 30, never relabels it");
        require(!chooseCameraMode({}, 30), "unknown capabilities stay unknown");
        require(!chooseCameraMode({{640,480,60,V4L2_PIX_FMT_MJPEG}},30), "do not choose a rate above the request");
        require(!chooseCameraMode({{0,480,30,V4L2_PIX_FMT_MJPEG}, {640,480,std::numeric_limits<double>::quiet_NaN(),V4L2_PIX_FMT_MJPEG}},30), "ignore invalid modes");
        require(cameraFormatName(V4L2_PIX_FMT_MJPEG) == "MJPG", "FOURCC is readable");
        std::cout << "Camera mode selection and fallback passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
