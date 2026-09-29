#include "camera_device.hpp"
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cerrno>
#include <tuple>

namespace {
int control(int fd, unsigned long request, void* value) {
    int result;
    do { result = ioctl(fd, request, value); } while (result < 0 && errno == EINTR);
    return result;
}
std::string devicePath(int index) { return "/dev/video" + std::to_string(index); }
double rate(v4l2_fract interval) {
    return interval.numerator ? double(interval.denominator) / interval.numerator : 0;
}
void intervals(int fd, uint32_t format, int width, int height, std::vector<CameraMode>& modes) {
    v4l2_frmivalenum interval{};
    interval.pixel_format = format; interval.width = width; interval.height = height;
    for (; control(fd, VIDIOC_ENUM_FRAMEINTERVALS, &interval) == 0; ++interval.index) {
        if (interval.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
            const double fps = rate(interval.discrete);
            if (fps > 0) modes.push_back({width, height, fps, format});
        } else {
            const auto& range = interval.stepwise;
            const double minTime = rate(range.min) > 0 ? 1 / rate(range.min) : 0;
            const double maxTime = rate(range.max) > 0 ? 1 / rate(range.max) : 0;
            const double step = rate(range.step) > 0 ? 1 / rate(range.step) : 0;
            for (int fps : {30, 60}) {
                const double time = 1.0 / fps;
                if (time < minTime - 1e-7 || time > maxTime + 1e-7) continue;
                if (interval.type == V4L2_FRMIVAL_TYPE_STEPWISE && step > 0 &&
                    std::abs((time - minTime) / step - std::round((time - minTime) / step)) > 1e-4) continue;
                modes.push_back({width, height, double(fps), format});
            }
            break;
        }
    }
}
}
CameraCapabilities cameraCapabilities(int index) {
    CameraCapabilities result;
    const int fd = open(devicePath(index).c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) { result.error = std::strerror(errno); return result; }
    v4l2_capability caps{};
    if (control(fd, VIDIOC_QUERYCAP, &caps) == 0) {
        const auto flags = caps.capabilities & V4L2_CAP_DEVICE_CAPS ? caps.device_caps : caps.capabilities;
        result.capture = (flags & V4L2_CAP_VIDEO_CAPTURE) && (flags & V4L2_CAP_STREAMING);
    }
    if (result.capture) {
        v4l2_fmtdesc format{}; format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        for (; control(fd, VIDIOC_ENUM_FMT, &format) == 0; ++format.index) {
            if (format.pixelformat != V4L2_PIX_FMT_MJPEG && format.pixelformat != V4L2_PIX_FMT_YUYV) continue;
            v4l2_frmsizeenum size{}; size.pixel_format = format.pixelformat;
            for (; control(fd, VIDIOC_ENUM_FRAMESIZES, &size) == 0; ++size.index) {
                if (size.type == V4L2_FRMSIZE_TYPE_DISCRETE) {
                    intervals(fd, format.pixelformat, size.discrete.width, size.discrete.height, result.modes);
                } else {
                    const auto& s = size.stepwise;
                    for (auto [w, h] : {std::pair{640u, 480u}, {640u, 360u}, {320u, 240u}, {1280u, 720u}}) {
                        if (w < s.min_width || w > s.max_width || h < s.min_height || h > s.max_height) continue;
                        if (size.type == V4L2_FRMSIZE_TYPE_STEPWISE &&
                            ((s.step_width && (w - s.min_width) % s.step_width) ||
                             (s.step_height && (h - s.min_height) % s.step_height))) continue;
                        intervals(fd, format.pixelformat, w, h, result.modes);
                    }
                    break;
                }
            }
        }
    }
    close(fd);
    return result;
}
std::optional<CameraMode> chooseCameraMode(const std::vector<CameraMode>& modes, int requestedFps, int width, int height) {
    std::optional<CameraMode> best;
    const auto score = [requestedFps, width, height](const CameraMode& m) {
        // Explicit resolution takes priority; automatic mode prefers FPS then 640x480.
        return std::tuple{width > 0 && (m.width != width || m.height != height) ? 1 : 0, std::abs(m.fps - requestedFps),
            std::abs(std::log(double(m.width) * m.height / (width > 0 ? double(width) * height : 640.0 * 480))),
            m.format == V4L2_PIX_FMT_MJPEG ? 0 : 1};
    };
    for (const auto& m : modes) {
        if (m.width <= 0 || m.height <= 0 || !std::isfinite(m.fps) || m.fps <= 0 || m.fps > requestedFps + 0.5) continue;
        if (m.format != V4L2_PIX_FMT_MJPEG && m.format != V4L2_PIX_FMT_YUYV) continue;
        if (!best || score(m) < score(*best)) best = m;
    }
    return best;
}
std::string cameraFormatName(uint32_t format) {
    std::string name(4, ' ');
    for (int i = 0; i < 4; ++i) name[i] = char((format >> (8 * i)) & 0xff);
    return name;
}
CameraExposure::CameraExposure(int index) {
    fd_ = open(devicePath(index).c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd_ < 0) return;
    v4l2_control value{}; value.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY;
    if (control(fd_, VIDIOC_G_CTRL, &value) != 0) return;
    original_ = value.value;
    if (original_ == 0) { fixed_ = true; return; }
    value.value = 0;
    if (control(fd_, VIDIOC_S_CTRL, &value) == 0) {
        changed_ = true;
        fixed_ = control(fd_, VIDIOC_G_CTRL, &value) == 0 && value.value == 0;
    }
}
CameraExposure::~CameraExposure() {
    if (fd_ < 0) return;
    if (changed_) {
        v4l2_control value{}; value.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY; value.value = original_;
        control(fd_, VIDIOC_S_CTRL, &value);
    }
    close(fd_);
}
