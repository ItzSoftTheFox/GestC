#pragma once
#include <linux/uinput.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

class MouseController {
public:
    MouseController(int screenWidth = 1920, int screenHeight = 1080);
    ~MouseController();

    void move(float normalizedX, float normalizedY);
    void click(bool pressed);
    void scroll(int steps);
    void rightClick(bool isDown);
    void middleClick(bool isDown);
private:
    int fd;
    int screenW;
    int screenH;
    
    bool setupDevice();
    void emitEvent(int type, int code, int val);
};