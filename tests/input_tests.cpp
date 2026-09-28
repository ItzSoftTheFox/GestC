#include "platform/input_device.hpp"
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdarg>
#include <cstring>
#include <cerrno>
#include <vector>
#include <set>
#include <iostream>
#include <stdexcept>

struct Recorded { int fd; input_event event; };
std::vector<Recorded> recorded;
std::set<int> live;
int nextFd = 1000;
bool failWrite = false;
extern "C" int __real_open(const char*, int, ...);
extern "C" ssize_t __real_write(int, const void*, size_t);
extern "C" int __real_close(int);
extern "C" int __real_ioctl(int, unsigned long, ...);
extern "C" int __wrap_open(const char* path, int flags, ...) {
    if (std::strcmp(path, "/dev/uinput") == 0) { live.insert(nextFd); return nextFd++; }
    mode_t mode = 0;
    if (flags & O_CREAT) { va_list args; va_start(args, flags); mode = va_arg(args, int); va_end(args); }
    return __real_open(path, flags, mode);
}
extern "C" ssize_t __wrap_write(int fd, const void* data, size_t size) {
    if (!live.count(fd)) return __real_write(fd, data, size);
    if (failWrite) { failWrite = false; errno = EIO; return -1; }
    if (size == sizeof(input_event)) recorded.push_back({fd, *static_cast<const input_event*>(data)});
    return static_cast<ssize_t>(size);
}
extern "C" int __wrap_ioctl(int fd, unsigned long request, ...) {
    if (live.count(fd)) return 0;
    va_list args; va_start(args, request); auto arg = va_arg(args, void*); va_end(args);
    return __real_ioctl(fd, request, arg);
}
extern "C" int __wrap_close(int fd) {
    if (live.erase(fd)) return 0;
    return __real_close(fd);
}
void require(bool condition, const char* text) { if (!condition) throw std::runtime_error(text); }
std::vector<Recorded> keys() {
    std::vector<Recorded> result;
    for (const auto& r : recorded) if (r.event.type == EV_KEY) result.push_back(r);
    return result;
}
int main() {
    try {
        InputDevice input; require(input.open(), "open virtual devices");
        require(live.size() == 2, "separate keyboard and pointer devices");
        handmouse::Command c; c.left = true; c.super = true;
        require(input.apply(c), "apply Super drag");
        auto k = keys(); require(k.size() == 2 && k[0].event.code == KEY_LEFTMETA && k[0].event.value == 1 && k[1].event.code == BTN_LEFT && k[1].event.value == 1, "modifier pressed before button");
        require(k[0].fd != k[1].fd, "modifier goes to keyboard");
        recorded.clear(); input.release(); k = keys();
        require(k.size() == 3 && k.front().event.code == BTN_LEFT && k.back().event.code == KEY_LEFTMETA, "button released before modifier");
        for (const auto& event : k) require(event.event.value == 0, "release does not press anything");
        recorded.clear(); c = {}; c.middleClick = true; require(input.apply(c), "middle click");
        k = keys(); require(k.size() == 2 && k[0].event.value == 1 && k[1].event.value == 0, "middle click is a pulse");
        recorded.clear(); c = {}; c.dx = .4;
        for (int i = 0; i < 3; ++i) require(input.apply(c), "fractional motion");
        int movement = 0;
        for (const auto& r : recorded) if (r.event.type == EV_REL && r.event.code == REL_X) movement += r.event.value;
        require(movement == 1, "fractional deltas accumulate");
        c = {}; c.left = c.super = true; input.apply(c);
        failWrite = true; require(!input.apply(c), "write failure surfaced");
        require(!input.ready() && live.empty(), "failure destroys both devices and releases modifiers");
        require(!input.error().isEmpty(), "failure has explanation");
        require(input.open(), "can reopen after failure"); input.close(); require(live.empty(), "close cleans up");
        std::cout << "Input sequencing, subpixel motion and failure cleanup passed.\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
