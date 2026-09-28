#include "input_device.hpp"
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <cmath>

InputDevice::~InputDevice() { close(); }
int InputDevice::createDevice(bool keyboard) {
    const int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        error_ = QStringLiteral("Nelze otevřít /dev/uinput: %1. Viz sekce Přístup ke vstupu v README.").arg(QString::fromLocal8Bit(std::strerror(errno)));
        return -1;
    }
    bool ok = ioctl(fd, UI_SET_EVBIT, EV_KEY) >= 0;
    if (keyboard) {
        // Advertise a standard keyboard so udev/libinput classify modifiers correctly.
        // The only key this application actually emits is KEY_LEFTMETA.
        for (int key = KEY_ESC; key <= KEY_MICMUTE; ++key) ok = (ioctl(fd, UI_SET_KEYBIT, key) >= 0) && ok;
    } else {
        for (int key : {BTN_LEFT, BTN_MIDDLE}) ok = (ioctl(fd, UI_SET_KEYBIT, key) >= 0) && ok;
        ok = (ioctl(fd, UI_SET_EVBIT, EV_REL) >= 0) && ok;
        for (int axis : {REL_X, REL_Y, REL_WHEEL}) ok = (ioctl(fd, UI_SET_RELBIT, axis) >= 0) && ok;
    }
    uinput_setup setup{};
    std::strncpy(setup.name, keyboard ? "HandMouse Virtual Keyboard" : "HandMouse Virtual Pointer", UINPUT_MAX_NAME_SIZE - 1);
    setup.id.bustype = BUS_USB; setup.id.vendor = 0x1209; setup.id.product = keyboard ? 2 : 1;
    ok = (ioctl(fd, UI_DEV_SETUP, &setup) >= 0) && ok;
    if (!ok || ioctl(fd, UI_DEV_CREATE) < 0) {
        error_ = QStringLiteral("Vytvoření virtuálního vstupu selhalo: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
        ::close(fd); return -1;
    }
    return fd;
}
bool InputDevice::open() {
    if (ready()) return true;
    error_.clear();
    pointerFd_ = createDevice(false);
    if (pointerFd_ < 0) return false;
    keyboardFd_ = createDevice(true);
    if (keyboardFd_ < 0) { close(); return false; }
    return true;
}
bool InputDevice::event(int fd, unsigned short type, unsigned short code, int value) {
    if (fd < 0) return false;
    input_event e{}; e.type = type; e.code = code; e.value = value;
    ssize_t written;
    do { written = ::write(fd, &e, sizeof(e)); } while (written < 0 && errno == EINTR);
    if (written == sizeof(e)) return true;
    error_ = QStringLiteral("Odeslání vstupu selhalo: %1").arg(QString::fromLocal8Bit(std::strerror(errno)));
    return false;
}
bool InputDevice::sync(int fd) { return event(fd, EV_SYN, SYN_REPORT, 0); }
bool InputDevice::apply(const handmouse::Command& c) {
    if (!ready()) return false;
    if (!std::isfinite(c.dx) || !std::isfinite(c.dy) || std::abs(c.dx) > 10000 || std::abs(c.dy) > 10000) {
        error_ = QStringLiteral("Neplatný pohyb kurzoru."); close(); return false;
    }
    bool ok = true;
    // Release mouse before Super; press Super before mouse, in separate reports.
    if (left_ && (!c.left || c.super != super_)) {
        ok = event(pointerFd_, EV_KEY, BTN_LEFT, 0) && sync(pointerFd_); left_ = false;
    }
    if (super_ != c.super) {
        ok = event(keyboardFd_, EV_KEY, KEY_LEFTMETA, c.super) && sync(keyboardFd_) && ok;
        super_ = c.super;
    }
    if (left_ != c.left) { ok = event(pointerFd_, EV_KEY, BTN_LEFT, c.left) && ok; left_ = c.left; }
    remainderX_ += c.dx; remainderY_ += c.dy;
    const int dx = static_cast<int>(remainderX_), dy = static_cast<int>(remainderY_);
    remainderX_ -= dx; remainderY_ -= dy;
    if (dx) ok = event(pointerFd_, EV_REL, REL_X, dx) && ok;
    if (dy) ok = event(pointerFd_, EV_REL, REL_Y, dy) && ok;
    if (c.scroll) ok = event(pointerFd_, EV_REL, REL_WHEEL, c.scroll) && ok;
    if (c.middleClick) {
        ok = event(pointerFd_, EV_KEY, BTN_MIDDLE, 1) && sync(pointerFd_) && ok;
        ok = event(pointerFd_, EV_KEY, BTN_MIDDLE, 0) && ok;
    }
    ok = sync(pointerFd_) && ok;
    if (!ok) close();
    return ok;
}
void InputDevice::release() {
    if (pointerFd_ >= 0) {
        event(pointerFd_, EV_KEY, BTN_LEFT, 0); event(pointerFd_, EV_KEY, BTN_MIDDLE, 0); sync(pointerFd_);
    }
    if (keyboardFd_ >= 0) { event(keyboardFd_, EV_KEY, KEY_LEFTMETA, 0); sync(keyboardFd_); }
    left_ = super_ = false; remainderX_ = remainderY_ = 0;
}
void InputDevice::close() {
    release();
    for (int fd : {pointerFd_, keyboardFd_}) if (fd >= 0) { ioctl(fd, UI_DEV_DESTROY); ::close(fd); }
    pointerFd_ = keyboardFd_ = -1;
}
