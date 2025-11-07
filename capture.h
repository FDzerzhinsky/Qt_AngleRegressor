#pragma once

// =============================================================================
// ВКЛЮЧЕНИЕ БИБЛИОТЕК
// =============================================================================
// Сначала стандартные библиотеки C++
#include <atomic>
#include <string>
#include <fstream>
#include <sstream>

// Затем OpenCV
#include <opencv2/opencv.hpp>

// Наши заголовки
#include "angle.h"

// Библиотека камеры (должна быть после OpenCV)
#include <MvCameraControl.h>

// =============================================================================
// СТРУКТУРА КОНФИГУРАЦИИ КАМЕРЫ
// =============================================================================
// Содержит параметры подключения к камере и ограничения на количество захватов
struct CameraConfig {
    std::string cameraIp;  // IP-адрес камеры
    std::string hostIp;    // IP-адрес сетевого интерфейса компьютера
    int max_captures = 0;  // Максимальное количество захватов (0 = без ограничений)
};

// =============================================================================
// СОСТОЯНИЕ КАМЕРЫ
// =============================================================================
// Содержит информацию о текущем состоянии подключения к камере
struct CameraState {
    void* handle = nullptr;  // Дескриптор камеры
    CameraConfig config;     // Конфигурация подключения
    unsigned char* pConvertBuffer = nullptr;  // Буфер для конвертированных изображений
    int nConvertBufferSize = 0;               // Размер буфера
    bool grabbingStarted = false;             // Флаг активности видеопотока
};

// =============================================================================
// КОНТЕКТС ДЛЯ CALLBACK-ФУНКЦИИ
// =============================================================================
// Содержит данные, передаваемые в callback-функцию обработки изображений
struct CallbackContext {
    std::atomic<int> count{ 0 };        // Счетчик захваченных кадров
    AngleContext* angle_context = nullptr;      // Контекст обработки изображений
    CameraState* cameraState = nullptr;         // Состояние камеры
    std::atomic<bool> stop{ false };    // Флаг остановки захвата
};

// =============================================================================
// ПРОТОТИПЫ ФУНКЦИЙ ДЛЯ РАБОТЫ С КАМЕРОЙ
// =============================================================================

// Основная функция захвата кадров
int capture(AngleContext& angle_context);

// Чтение конфигурации камеры из файла
CameraConfig read_camera_config(const std::string& filename = "config.ini");

// Генерация имени снэпшота на основе текущего времени
std::string generateSnapshotName();

// Инициализация камеры
CameraState* InitCamera();

// Деинициализация камеры
void DeinitCamera(CameraState* state);

// Запуск захвата видео
int StartGrabbing(CameraState* state);

// Остановка захвата видео
int StopGrabbing(CameraState* state);

// Callback-функция для обработки захваченных изображений
void __stdcall ImageCallbackEx(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser);

// Функция ожидания нажатия клавиши
void WaitForKeyPress(void) noexcept;