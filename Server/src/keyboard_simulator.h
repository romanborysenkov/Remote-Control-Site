#pragma once

#include <string>

class KeyboardSimulator {
public:
    enum class ArrowKey {
        UP,
        DOWN,
        LEFT,
        RIGHT
    };

    // Симулює натискання стрілочки
    static bool simulateArrowKey(ArrowKey key);
    
    // Симулює натискання клавіші за її назвою
    static bool simulateKey(const std::string& keyName);
    
private:
    // Внутрішні методи для роботи з Core Graphics
    static void keyDown(int keyCode);
    static void keyUp(int keyCode);
    static int getKeyCode(ArrowKey key);
};
