// =============================================================================
// ВКЛЮЧЕНИЕ БИБЛИОТЕК
// =============================================================================
#include "capture.h"
#include "platform_utils.h"

#include <stdio.h>
#include <chrono>
#include <thread>

using namespace std;
using namespace chrono;

// =============================================================================
// ФУНКЦИЯ ОЖИДАНИЯ НАЖАТИЯ КЛАВИШИ
// =============================================================================
// Ожидает нажатия любой клавиши для приостановки выполнения программы
// Используется для удобства отладки и тестирования
void WaitForKeyPress(void) noexcept {
    printf("Press any key to continue...\n");
    // Упрощенная версия без Windows API
    std::cin.get();
}

// =============================================================================
// ЧТЕНИЕ КОНФИГУРАЦИИ КАМЕРЫ ИЗ ФАЙЛА
// =============================================================================
// Эта функция читает параметры камеры из конфигурационного файла
// Позволяет настраивать подключение к камеры без перекомпиляции кода
CameraConfig read_camera_config(const std::string& filename) {
    // Значения по умолчанию на случай отсутствия файла конфигурации
    CameraConfig config = { "192.168.1.100", "192.168.1.101", 0 };

    // Открываем файл конфигурации для чтения
    ifstream file(filename);
    if (file.is_open()) {
        string line;
        // Читаем файл построчно
        while (getline(file, line)) {
            // Пропускаем пустые строки и комментарии
            if (line.empty() || line[0] == '#') continue;

            // Используем stringstream для разбора строки
            istringstream iss(line);
            string key, value;
            // Разбираем строку на ключ и значение по знаку '='
            if (getline(iss, key, '=') && getline(iss, value)) {
                // Удаляем пробелы в начале и конце ключа и значения
                key.erase(0, key.find_first_not_of(" \t"));
                key.erase(key.find_last_not_of(" \t") + 1);
                value.erase(0, value.find_first_not_of(" \t"));
                value.erase(value.find_last_not_of(" \t") + 1);

                // Записываем значения в конфигурацию в зависимости от ключа
                // Используем блоки try-catch для обработки ошибок преобразования типов
                try {
                    if (key == "camera_ip") config.cameraIp = value;
                    else if (key == "host_ip") config.hostIp = value;
                    else if (key == "max_captures") config.max_captures = stoi(value);
                }
                catch (const exception& e) {
                    // Выводим ошибку, но продолжаем работу с остальными параметрами
                    printf("Error parsing config value: %s = %s\n", key.c_str(), value.c_str());
                    printf("Error message: %s\n", e.what());
                }
            }
        }
        file.close();
    }
    else {
        // Если файл не найден, используем значения по умолчанию
        printf("Config file not found, using default camera values\n");
    }

    return config;
}

// =============================================================================
// ПРЕОБРАЗОВАНИЕ СТРОКИ IP В ЧИСЛОВОЙ ФОРМАТ
// =============================================================================
// Преобразует строковое представление IP-адреса в числовой формат
// для сравнения с IP-адресами, возвращаемыми SDK камеры
unsigned int ParseIpAddress(const string& ipStr) {
    unsigned int nIp1, nIp2, nIp3, nIp4;
    // Парсим строку IP-адреса на составляющие
    sscanf(ipStr.c_str(), "%d.%d.%d.%d", &nIp1, &nIp2, &nIp3, &nIp4);
    // Формируем 32-битное представление IP-адреса
    return (nIp1 << 24) | (nIp2 << 16) | (nIp3 << 8) | nIp4;
}

// =============================================================================
// ПРОВЕРКА И НАСТРОЙКА ТРИГГЕРНОГО РЕЖИМА КАМЕРЫ
// =============================================================================
// Проверяет текущие настройки триггерного режима камеры и при необходимости
// настраивает их для работы с внешним триггером
bool ConfigureTriggerMode(CameraState* state) {
    // Проверяем валидность указателя и дескриптора камеры
    if (!state || !state->handle) return false;

    int nRet = MV_OK;
    MVCC_ENUMVALUE stEnumValue = { 0 };

    // Проверяем текущий режим триггера
    nRet = MV_CC_GetEnumValue(state->handle, "TriggerMode", &stEnumValue);
    if (nRet != MV_OK) {
        printf("Failed to get TriggerMode! nRet [0x%x]\n", nRet);
        return false;
    }

    printf("Current TriggerMode value: %d\n", stEnumValue.nCurValue);

    // Если режим не внешний триггер, настраиваем его
    if (stEnumValue.nCurValue != 1) { // 1 = MV_TRIGGER_MODE_ON
        printf("Setting TriggerMode to ON...\n");
        nRet = MV_CC_SetEnumValue(state->handle, "TriggerMode", 1);
        if (nRet != MV_OK) {
            printf("Failed to set TriggerMode! nRet [0x%x]\n", nRet);
            return false;
        }
    }

    // Проверяем источник триггера
    nRet = MV_CC_GetEnumValue(state->handle, "TriggerSource", &stEnumValue);
    if (nRet != MV_OK) {
        printf("Failed to get TriggerSource! nRet [0x%x]\n", nRet);
        return false;
    }

    printf("Current TriggerSource value: %d\n", stEnumValue.nCurValue);

    // Если источник не Line0, настраиваем его
    if (stEnumValue.nCurValue != 0) { // 0 = MV_TRIGGER_SOURCE_LINE0
        printf("Setting TriggerSource to Line0...\n");
        nRet = MV_CC_SetEnumValue(state->handle, "TriggerSource", 0);
        if (nRet != MV_OK) {
            printf("Failed to set TriggerSource! nRet [0x%x]\n", nRet);
            return false;
        }
    }

    return true;
}

// =============================================================================
// ИНИЦИАЛИЗАЦИЯ КАМЕРЫ
// =============================================================================
// Выполняет все необходимые действия для инициализации и настройки камеры:
// - Загружает конфигурацию из файла
// - Инициализирует SDK камеры
// - Находит камеру с указанным IP-адресом
// - Настраивает триггерный режим
// - Оптимизирует размер пакета для GigE камер
CameraState* InitCamera() {
    // Создаем новое состояние камеры
    CameraState* state = new CameraState();
    // Загружаем конфигурацию из файла
    state->config = read_camera_config();
    int nRet = MV_OK;

    // Выводим информацию о загруженной конфигурации
    printf("Using camera IP: %s\n", state->config.cameraIp.c_str());
    printf("Using host IP: %s\n", state->config.hostIp.c_str());
    printf("Max captures: %d\n", state->config.max_captures);

    // Инициализация SDK камеры
    nRet = MV_CC_Initialize();
    if (MV_OK != nRet) {
        printf("Initialize SDK fail! nRet [0x%x]\n", nRet);
        delete state;
        return nullptr;
    }

    // Перечисление доступных устройств
    MV_CC_DEVICE_INFO_LIST stDeviceList;
    memset(&stDeviceList, 0, sizeof(MV_CC_DEVICE_INFO_LIST));
    nRet = MV_CC_EnumDevices(MV_GIGE_DEVICE, &stDeviceList);
    if (MV_OK != nRet) {
        printf("Enum Devices fail! nRet [0x%x]\n", nRet);
        MV_CC_Finalize();
        delete state;
        return nullptr;
    }

    if (stDeviceList.nDeviceNum == 0) {
        printf("No devices found!\n");
        MV_CC_Finalize();
        delete state;
        return nullptr;
    }

    // Поиск камеры с нужным IP
    unsigned int nTargetIp = ParseIpAddress(state->config.cameraIp);
    bool bFound = false;

    // Перебираем все найденные устройства
    for (unsigned int i = 0; i < stDeviceList.nDeviceNum; i++) {
        MV_CC_DEVICE_INFO* pDeviceInfo = stDeviceList.pDeviceInfo[i];
        // Проверяем, что устройство является GigE-камерой
        if (pDeviceInfo->nTLayerType == MV_GIGE_DEVICE) {
            unsigned int nCurrentIp = pDeviceInfo->SpecialInfo.stGigEInfo.nCurrentIp;

            // Сравниваем IP-адрес камеры с целевым
            if (nCurrentIp == nTargetIp) {
                printf("Found camera with matching IP\n");

                // Пробуем разные режимы доступа к камере
                int accessModes[] = {
                    MV_ACCESS_Exclusive,
                    MV_ACCESS_Control,
                    MV_ACCESS_Monitor
                };

                const char* modeNames[] = {
                    "Exclusive",
                    "Control",
                    "Monitor"
                };

                // Перебираем все режимы доступа
                for (int j = 0; j < sizeof(accessModes) / sizeof(accessModes[0]); j++) {
                    // Проверка доступности камеры в данном режиме
                    bool bAccessible = MV_CC_IsDeviceAccessible(pDeviceInfo, accessModes[j]);
                    if (!bAccessible) {
                        printf("Camera is NOT accessible in %s mode.\n", modeNames[j]);
                        continue;
                    }

                    printf("Camera is accessible in %s mode. Trying to open...\n", modeNames[j]);

                    // Создание дескриптора камеры
                    nRet = MV_CC_CreateHandle(&state->handle, pDeviceInfo);
                    if (MV_OK != nRet) {
                        printf("Create Handle fail! nRet[0x%x]\n", nRet);
                        continue;
                    }

                    // Попытка открыть камеру
                    nRet = MV_CC_OpenDevice(state->handle, accessModes[j], 0);
                    if (MV_OK == nRet) {
                        printf("Camera opened successfully in %s mode.\n", modeNames[j]);
                        bFound = true;
                        break;
                    }
                    else {
                        printf("Open Device fail in %s mode! nRet [0x%x]\n", modeNames[j], nRet);
                        MV_CC_DestroyHandle(state->handle);
                        state->handle = nullptr;
                    }
                }

                if (bFound) break;
            }
        }
    }

    if (!bFound || state->handle == nullptr) {
        printf("Failed to open camera with IP: %s\n", state->config.cameraIp.c_str());
        MV_CC_Finalize();
        delete state;
        return nullptr;
    }

    // Настройка триггерного режима
    if (!ConfigureTriggerMode(state)) {
        printf("Failed to configure trigger mode!\n");
        MV_CC_CloseDevice(state->handle);
        MV_CC_DestroyHandle(state->handle);
        MV_CC_Finalize();
        delete state;
        return nullptr;
    }

    // Оптимизация размера пакета для GigE камер
    int nPacketSize = MV_CC_GetOptimalPacketSize(state->handle);
    if (nPacketSize > 0) {
        nRet = MV_CC_SetIntValueEx(state->handle, "GevSCPSPacketSize", nPacketSize);
        if (nRet != MV_OK) {
            printf("Warning: Set Packet Size fail nRet [0x%x]!\n", nRet);
        }
    }

    return state;
}

// =============================================================================
// ЗАПУСК ЗАХВАТА ВИДЕО
// =============================================================================
// Запускает процесс захвата видео с камеры
// Камера начинает ожидать внешние триггеры для захвата кадров
int StartGrabbing(CameraState* state) {
    // Проверяем валидность указателя и дескриптора камеры
    if (!state || !state->handle) return -1;

    // Проверяем, не запущен ли уже захват
    if (state->grabbingStarted) {
        printf("Grabbing already started\n");
        return MV_OK;
    }

    // Запускаем захват видео
    int nRet = MV_CC_StartGrabbing(state->handle);
    if (MV_OK != nRet) {
        printf("Start Grabbing fail! nRet [0x%x]\n", nRet);
        return nRet;
    }

    state->grabbingStarted = true;
    printf("Grabbing started, waiting for external trigger...\n");
    return MV_OK;
}

// =============================================================================
// ОСТАНОВКА ЗАХВАТА ВИДЕО
// =============================================================================
// Останавливает процесс захвата видео с камеры
int StopGrabbing(CameraState* state) {
    // Проверяем валидность указателя и дескриптора камеры
    if (!state || !state->handle) return -1;

    // Проверяем, запущен ли захват
    if (!state->grabbingStarted) {
        printf("Grabbing not started\n");
        return MV_OK;
    }

    // Останавливаем захват видео
    int nRet = MV_CC_StopGrabbing(state->handle);
    if (MV_OK != nRet) {
        printf("Stop Grabbing fail! nRet [0x%x]\n", nRet);
        return nRet;
    }

    state->grabbingStarted = false;
    printf("Grabbing stopped\n");
    return MV_OK;
}

// =============================================================================
// ДЕИНИЦИАЛИЗАЦИЯ КАМЕРЫ
// =============================================================================
// Корректно освобождает все ресурсы, связанные с камерой
void DeinitCamera(CameraState* state) {
    if (!state) return;

    // Останавливаем захват, если он активен
    if (state->grabbingStarted) {
        StopGrabbing(state);
    }

    // Закрываем устройство
    if (state->handle) {
        MV_CC_CloseDevice(state->handle);
        MV_CC_DestroyHandle(state->handle);
    }

    // Освобождаем буфер конвертации
    if (state->pConvertBuffer) {
        delete[] state->pConvertBuffer;
        state->pConvertBuffer = nullptr;
    }

    // Финализируем SDK
    MV_CC_Finalize();

    delete state;
}

// =============================================================================
// ГЕНЕРАЦИЯ ИМЕНИ СНЭПШОТА НА ОСНОВЕ ТЕКУЩЕГО ВРЕМЕНИ
// =============================================================================
// Создает уникальное имя снэпшота в формате "Snapshot_YYYYMMDD_HHMMSS"
// Используется для идентификации кадров при отправке через сокет
std::string generateSnapshotName() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::tm tm;
    PlatformUtils::LocalTime(in_time_t, tm);

    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "Snapshot_%Y%m%d_%H%M%S", &tm);
    return std::string(buffer);
}

// =============================================================================
// CALLBACK-ФУНКЦИЯ ДЛЯ ОБРАБОТКИ ЗАХВАЧЕННЫХ ИЗОБРАЖЕНИЙ
// =============================================================================
// Эта функция вызывается автоматически при получении нового кадра с камеры
// Важно: эта функция выполняется в потоке драйвера камеры, поэтому должна
// работать максимально быстро и не блокировать надолго
void __stdcall ImageCallbackEx(unsigned char* pData, MV_FRAME_OUT_INFO_EX* pFrameInfo, void* pUser) {
    // Проверяем, что все параметры валидны
    if (pFrameInfo && pData && pUser) {
        // Преобразуем указатель на пользовательские данные в наш контекст
        CallbackContext* ctx = (CallbackContext*)pUser;
        CameraState* state = ctx->cameraState;

        // Проверяем флаг остановки - если установлен, немедленно выходим
        if (ctx->stop) {
            return;
        }

        // Замер времени начала обработки кадра для оценки производительности
        auto capture_start = high_resolution_clock::now();

        // =====================================================================
        // ПОДГОТОВКА БУФЕРА ДЛЯ КОНВЕРТАЦИИ ФОРМАТА ПИКСЕЛЕЙ
        // =====================================================================
        // Камеры часто используют специализированные форматы пикселей
        // Нам нужно преобразовать их в стандартный BGR формат для OpenCV
        int requiredBufferSize = pFrameInfo->nWidth * pFrameInfo->nHeight * 3;

        // Перевыделяем буфер при необходимости (если размер изображения изменился)
        if (state->nConvertBufferSize < requiredBufferSize) {
            if (state->pConvertBuffer) {
                delete[] state->pConvertBuffer; // Освобождаем старый буфер
            }
            // Выделяем новый буфер достаточного размера
            state->pConvertBuffer = new unsigned char[requiredBufferSize];
            state->nConvertBufferSize = requiredBufferSize;
            printf("Reallocated conversion buffer: %d bytes\n", requiredBufferSize);
        }

        // =====================================================================
        // НАСТРОЙКА ПАРАМЕТРОВ КОНВерТАЦИИ ФОРМАТА ПИКСЕЛЕЙ
        // =====================================================================
        MV_CC_PIXEL_CONVERT_PARAM stConvertParam = { 0 };
        stConvertParam.nWidth = pFrameInfo->nWidth;          // Ширина изображения
        stConvertParam.nHeight = pFrameInfo->nHeight;        // Высота изображения
        stConvertParam.pSrcData = pData;                     // Исходные данные
        stConvertParam.nSrcDataLen = pFrameInfo->nFrameLen;  // Длина исходных данных
        stConvertParam.enSrcPixelType = pFrameInfo->enPixelType; // Исходный формат
        stConvertParam.enDstPixelType = PixelType_Gvsp_BGR8_Packed; // Целевой формат
        stConvertParam.pDstBuffer = state->pConvertBuffer;   // Выходной буфер
        stConvertParam.nDstBufferSize = state->nConvertBufferSize; // Размер буфера

        // =====================================================================
        // ВЫПОЛНЕНИЕ КОНВЕРТАЦИИ ФОРМАТА ПИКСЕЛЕЙ
        // =====================================================================
        int nRet = MV_CC_ConvertPixelType(state->handle, &stConvertParam);

        // Замер времени завершения обработки для оценки производительности
        auto capture_end = high_resolution_clock::now();
        auto capture_time = duration_cast<microseconds>(capture_end - capture_start).count();

        // Если конвертация прошла успешно
        if (MV_OK == nRet) {
            // =================================================================
            // СОЗДАНИЕ OPENCV ИЗОБРАЖЕНИЯ ИЗ БУФЕРА
            // =================================================================
            // Создаем объект Mat из буфера с данными
            // CV_8UC3: 8-бит беззнаковое, 3 канала (BGR)
            cv::Mat frame = cv::Mat(
                stConvertParam.nHeight,    // Высота изображения
                stConvertParam.nWidth,     // Ширина изображения
                CV_8UC3,                   // Формат данных (8-бит BGR)
                state->pConvertBuffer      // Указатель на данные
            );

            // =================================================================
            // БЛОКИРОВКА ДОСТУПА К ОБЩИМ ДАННЫМ
            // =================================================================
            // Используем мьютекс для безопасной записи в общий контекст
            std::lock_guard<std::mutex> lock(ctx->angle_context->data_mutex);

            // Сохраняем временные метки захвата
            ctx->angle_context->capture_start_time = capture_start;
            ctx->angle_context->capture_end_time = high_resolution_clock::now();

            // Сохраняем кадр для обработки в основном потоке
            // Используем clone() для создания независимой копии
            ctx->angle_context->latest_frame = frame.clone();

            // Генерируем и сохраняем имя снэпшота для этого кадра
            ctx->angle_context->latest_snapshot_name = generateSnapshotName();

            // Устанавливаем флаг, что новый кадр доступен для обработки
            ctx->angle_context->new_frame_available = true;

            // Сбрасываем флаг завершения обработки
            ctx->angle_context->processing_complete = false;

            // Вычисляем время захвата
            auto capture_duration = duration_cast<microseconds>(
                ctx->angle_context->capture_end_time - ctx->angle_context->capture_start_time
            );
            ctx->angle_context->capture_time = capture_duration.count() / 1000.0;

            // Вывод времени обработки кадра для мониторинга производительности
            printf("Frame captured and converted in %f ms\n", ctx->angle_context->capture_time);

            // Увеличиваем счетчик захваченных кадров
            ctx->count++;

            // =================================================================
            // СОХРАНЕНИЕ СНЭПШОТА ДЛЯ ОТЛАДКИ (ЕСЛИ ВКЛЮЧЕНО)
            // =================================================================
            // Эта операция может быть медленной, поэтому выполняется только
            // при явном указании флага --save-snapshots
            if (ctx->angle_context->save_snapshots) {
                // Замер времени начала сохранения для оценки производительности
                auto save_start = high_resolution_clock::now();

                // Формируем имя файла на основе номера кадра
                char filename[50];
                snprintf(filename, sizeof(filename), "snaps/snapshot_%d.png", ctx->count.load());

                // Сохраняем изображение в файл (это операция ввода-вывода, может быть медленной)
                cv::imwrite(filename, frame);

                // Замер времени завершения сохранения
                auto save_end = high_resolution_clock::now();
                auto save_time = duration_cast<microseconds>(save_end - save_start).count();

                // Вывод информации о сохранении файла
                printf("Saved %s (%f ms)\n", filename, (double)save_time / 1000);
            }
        }
        else {
            // Вывод ошибки, если конвертация не удалась
            printf("Convert pixel type failed! nRet [0x%x]\n", nRet);
        }
    }
}