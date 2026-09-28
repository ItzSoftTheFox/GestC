#pragma once
#include "gestures/gesture_engine.hpp"
#include <QString>

class InputDevice {
public:
    InputDevice() = default;
    InputDevice(const InputDevice&) = delete;
    InputDevice& operator=(const InputDevice&) = delete;
    ~InputDevice();
    bool open();
    bool apply(const handmouse::Command& command);
    void release();
    void close();
    bool ready() const { return pointerFd_ >= 0 && keyboardFd_ >= 0; }
    QString error() const { return error_; }
private:
    int createDevice(bool keyboard);
    bool event(int fd, unsigned short type, unsigned short code, int value);
    bool sync(int fd);
    int pointerFd_ = -1, keyboardFd_ = -1;
    bool left_ = false, super_ = false;
    double remainderX_ = 0, remainderY_ = 0;
    QString error_;
};
