#include "keyboard_simulator.h"
#include "logger.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <X11/keysymdef.h>
#include <X11/extensions/XTest.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#include <unistd.h>
#endif

bool KeyboardSimulator::simulateArrowKey(ArrowKey key) {
#if defined(_WIN32)
    int vk = getKeyCode(key);
    if (vk == -1) return false;
    INPUT down = {};
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = static_cast<WORD>(vk);
    INPUT up = {};
    up.type = INPUT_KEYBOARD;
    up.ki.wVk = static_cast<WORD>(vk);
    up.ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(1, &down, sizeof(INPUT));
    Sleep(50);
    SendInput(1, &up, sizeof(INPUT));
    return true;

#elif defined(__linux__)
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        Logger::error("XOpenDisplay failed");
        return false;
    }
    int keycode = getKeyCodeX11(display, key);
    if (keycode == -1) {
        XCloseDisplay(display);
        return false;
    }
    XTestFakeKeyEvent(display, keycode, True, CurrentTime);
    XFlush(display);
    usleep(50000);
    XTestFakeKeyEvent(display, keycode, False, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);
    return true;

#elif defined(__APPLE__)
    int keyCode = getKeyCode(key);
    if (keyCode == -1) return false;
    keyDown(keyCode);
    usleep(50000);
    keyUp(keyCode);
    return true;

#else
    (void)key;
    Logger::warning("Симуляція клавіатури не підтримується на цій платформі");
    return false;
#endif
}

bool KeyboardSimulator::simulateKey(const std::string& keyName) {
    ArrowKey key;
    if (keyName == "up" || keyName == "UP") {
        key = ArrowKey::UP;
    } else if (keyName == "down" || keyName == "DOWN") {
        key = ArrowKey::DOWN;
    } else if (keyName == "left" || keyName == "LEFT") {
        key = ArrowKey::LEFT;
    } else if (keyName == "right" || keyName == "RIGHT") {
        key = ArrowKey::RIGHT;
    } else {
        Logger::warning("Невідома команда: " + keyName);
        return false;
    }
    return simulateArrowKey(key);
}

#if defined(_WIN32)
void KeyboardSimulator::keyDown(int) {}
void KeyboardSimulator::keyUp(int) {}
int KeyboardSimulator::getKeyCode(ArrowKey key) {
    switch (key) {
        case ArrowKey::UP:    return VK_UP;     // 0x26
        case ArrowKey::DOWN:  return VK_DOWN;    // 0x28
        case ArrowKey::LEFT:  return VK_LEFT;    // 0x25
        case ArrowKey::RIGHT: return VK_RIGHT;   // 0x27
        default: return -1;
    }
}
#endif

#if defined(__linux__)
void KeyboardSimulator::keyDown(int) {}
void KeyboardSimulator::keyUp(int) {}
static int getKeyCodeX11(Display* display, ArrowKey key) {
    KeySym ks;
    switch (key) {
        case ArrowKey::UP:    ks = XK_Up;    break;
        case ArrowKey::DOWN:  ks = XK_Down;  break;
        case ArrowKey::LEFT:  ks = XK_Left;  break;
        case ArrowKey::RIGHT: ks = XK_Right; break;
        default: return -1;
    }
    int kc = XKeysymToKeycode(display, ks);
    return (kc != 0) ? kc : -1;
}

int KeyboardSimulator::getKeyCode(ArrowKey) {
    return -1; // unused on Linux
}
#endif

#if defined(__APPLE__)
void KeyboardSimulator::keyDown(int keyCode) {
    CGEventRef keyDownEvent = CGEventCreateKeyboardEvent(nullptr, keyCode, true);
    if (keyDownEvent) {
        CGEventPost(kCGHIDEventTap, keyDownEvent);
        CFRelease(keyDownEvent);
    } else {
        Logger::error("Помилка створення події натискання клавіші (код: " + std::to_string(keyCode) + ")");
    }
}

void KeyboardSimulator::keyUp(int keyCode) {
    CGEventRef keyUpEvent = CGEventCreateKeyboardEvent(nullptr, keyCode, false);
    if (keyUpEvent) {
        CGEventPost(kCGHIDEventTap, keyUpEvent);
        CFRelease(keyUpEvent);
    } else {
        Logger::error("Помилка створення події відпускання клавіші (код: " + std::to_string(keyCode) + ")");
    }
}

int KeyboardSimulator::getKeyCode(ArrowKey key) {
    switch (key) {
        case ArrowKey::UP:    return 126; // kVK_UpArrow
        case ArrowKey::DOWN:  return 125; // kVK_DownArrow
        case ArrowKey::LEFT:  return 123; // kVK_LeftArrow
        case ArrowKey::RIGHT: return 124; // kVK_RightArrow
        default: return -1;
    }
}
#endif
