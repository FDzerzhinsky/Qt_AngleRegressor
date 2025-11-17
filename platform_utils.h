#pragma once

// =============================================================================
// КРОССПЛАТФОРМЕННЫЕ УТИЛИТЫ
// =============================================================================
// Заменяет Windows-specific функции на кроссплатформенные аналоги. 

#include <chrono>
#include <thread>
#include <iostream>
#include <ctime>

class PlatformUtils {
public:
    // Замена Sleep
    static void Sleep(int ms) {
        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    }

    // Замена _kbhit (упрощенная версия)
    static bool KeyPressed() {
        // Для Windows будем использовать альтернативный подход
#ifdef _WIN32
        return false; // Упрощаем для избежания конфликтов
#else
        return false;
#endif
    }

    // Замена _getch
    static int GetKey() {
        return 0; // Упрощаем
    }

    // Замена localtime_s
    static void LocalTime(std::time_t time, std::tm& tm) {
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif
    }
};