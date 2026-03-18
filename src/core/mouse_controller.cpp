#include "mouse_controller.hpp"

MouseController::MouseController(int screenWidth, int screenHeight) 
    : fd(-1), screenW(screenWidth), screenH(screenHeight) {
    setupDevice();
}

MouseController::~MouseController() {
    if (fd >= 0) {
        ioctl(fd, UI_DEV_DESTROY);
        close(fd);
    }
}

bool MouseController::setupDevice() {
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        std::cerr << "Chyba: Nelze otevrit /dev/uinput. Pravdepodobne chybí prava (zkus sudo)." << std::endl;
        return false;
    }

    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    ioctl(fd, UI_SET_ABSBIT, ABS_X);
    ioctl(fd, UI_SET_ABSBIT, ABS_Y);

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
    ioctl(fd, UI_SET_KEYBIT, BTN_RIGHT);
    ioctl(fd, UI_SET_KEYBIT, BTN_MIDDLE);

    // Enable relative events and the hardware scroll wheel axis
    ioctl(fd, UI_SET_EVBIT, EV_REL);
    ioctl(fd, UI_SET_RELBIT, REL_WHEEL);

    struct uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));
    
    // Pojmenování našeho virtuálního hardwaru
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "HandMouse Virtual Pointer");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1234;
    uidev.id.product = 0x5678;
    uidev.id.version = 1;

    uidev.absmin[ABS_X] = 0;
    uidev.absmax[ABS_X] = screenW;
    uidev.absmin[ABS_Y] = 0;
    uidev.absmax[ABS_Y] = screenH;

    write(fd, &uidev, sizeof(uidev));
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        std::cerr << "Chyba: Nelze vytvorit uinput zarizeni." << std::endl;
        return false;
    }

    return true;
}

void MouseController::emitEvent(int type, int code, int val) {
    if (fd < 0) return;
    
    struct input_event ie;
    memset(&ie, 0, sizeof(ie));
    ie.type = type;
    ie.code = code;
    ie.value = val;
    
    write(fd, &ie, sizeof(ie));
}

void MouseController::move(float normalizedX, float normalizedY) {
    if (fd < 0) return;

    // Přepočet procentuální pozice kamery na reálné pixely monitoru
    int pixelX = static_cast<int>(normalizedX * screenW);
    int pixelY = static_cast<int>(normalizedY * screenH);

    // Omezení, aby kurzor nevyjel mimo monitor
    pixelX = std::max(0, std::min(pixelX, screenW));
    pixelY = std::max(0, std::min(pixelY, screenH));

    // Odeslání X, Y a potvrzení synchronizace
    emitEvent(EV_ABS, ABS_X, pixelX);
    emitEvent(EV_ABS, ABS_Y, pixelY);
    emitEvent(EV_SYN, SYN_REPORT, 0);
}

void MouseController::click(bool pressed) {
    if (fd < 0) return;

    // Send the button state. 1 means pressed down. 0 means released.
    emitEvent(EV_KEY, BTN_LEFT, pressed ? 1 : 0);
    
    // Always send a sync event to force the kernel to process the action
    emitEvent(EV_SYN, SYN_REPORT, 0);
}

void MouseController::rightClick(bool isDown) {
    if (fd < 0) return;
    
    // Send the right button event directly to the virtual input device
    emitEvent(EV_KEY, BTN_RIGHT, isDown ? 1 : 0);
    emitEvent(EV_SYN, SYN_REPORT, 0);
}

void MouseController::middleClick(bool isDown) {
    if (fd < 0) return;
    emitEvent(EV_KEY, BTN_MIDDLE, isDown ? 1 : 0);
    emitEvent(EV_SYN, SYN_REPORT, 0);
}

void MouseController::scroll(int steps) {
    if (fd < 0 || steps == 0) return;

    // Send a relative event for the scroll wheel
    // Positive steps scroll up, negative steps scroll down
    emitEvent(EV_REL, REL_WHEEL, steps);
    emitEvent(EV_SYN, SYN_REPORT, 0);
}