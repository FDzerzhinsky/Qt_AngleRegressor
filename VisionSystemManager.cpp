// [file name]: VisionSystemManager.cpp
// =============================================================================
// ВКЛЮЧЕНИЕ БИБЛИОТЕК
// =============================================================================
#include "VisionSystemManager.h"
#include "angle.h"
#include "capture.h"

#include <QDebug>
#include <QMetaObject>
#include <QDir>
#include <QImage>
#include <QDateTime>
#include <chrono>
#include <thread>
#include <future>

using namespace std::chrono_literals;

VisionSystemManager::VisionSystemManager(QObject* parent)
    : QObject(parent)
    , m_visionRunning(false)
    , m_cameraState(nullptr)
{
    qDebug() << "VisionSystemManager created";
}

VisionSystemManager::~VisionSystemManager()
{
    stopVisionSystem();
}

// =============================================================================
// НОВАЯ ФУНКЦИЯ ДЛЯ ГЕНЕРАЦИИ УНИКАЛЬНОГО ИМЕНИ СНЭПШОТА
// =============================================================================
QString VisionSystemManager::generateSnapshotName()
{
    // Получаем текущее время с точностью до секунды
    QDateTime currentTime = QDateTime::currentDateTime();

    // Форматируем время в строку: "snap_год-месяц-день_час-минута-секунда"
    // Используем дефисы вместо точек и двоеточий для совместимости с файловыми системами
    QString timestamp = currentTime.toString("yyyy-MM-dd_hh-mm-ss");

    // Добавляем префикс и возвращаем
    return QString("snap_%1").arg(timestamp);
}

void VisionSystemManager::startVisionSystem()
{
    qDebug() << "VisionSystemManager::startVisionSystem() called";

    if (m_visionRunning) {
        qDebug() << "Vision system is already running";
        emit visionLogMessage("Vision system is already running");
        return;
    }

    try {
        emit visionLogMessage("Initializing vision system...");
        qDebug() << "Initializing vision system...";

        // Инициализируем систему компьютерного зрения
        initializeVisionSystem();

        m_visionRunning = true;

        // Запускаем основной цикл в отдельном std::thread
        m_visionThread = std::thread(&VisionSystemManager::visionMainLoop, this);

        emit visionStatusChanged(true);
        emit visionLogMessage("Vision system started successfully");
        qDebug() << "Vision system started successfully";

    }
    catch (const std::exception& e) {
        QString errorMsg = QString("Failed to start vision system: %1").arg(e.what());
        qDebug() << errorMsg;
        emit visionError(errorMsg);
    }
}

void VisionSystemManager::stopVisionSystem()
{
    qDebug() << "VisionSystemManager::stopVisionSystem() called";

    if (!m_visionRunning) {
        qDebug() << "Vision system is not running";
        return;
    }

    emit visionLogMessage("Stopping vision system...");
    qDebug() << "Stopping vision system...";
    m_visionRunning = false;

    // Устанавливаем флаг остановки в callback контексте
    if (m_callbackContext) {
        m_callbackContext->stop = true;
    }

    if (m_visionThread.joinable()) {
        m_visionThread.join();
        qDebug() << "Vision thread joined";
    }

    cleanupVisionSystem();

    emit visionStatusChanged(false);
    emit visionLogMessage("Vision system stopped");
    qDebug() << "Vision system stopped";
}

void VisionSystemManager::configureVisionSystem(bool saveSnapshots)
{
    std::lock_guard<std::mutex> lock(m_dataMutex);
    if (m_angleContext) {
        m_angleContext->save_snapshots = saveSnapshots;
    }
}

// =============================================================================
// НОВЫЙ МЕТОД ДЛЯ УСТАНОВКИ ФЛАГА СОХРАНЕНИЯ СНЭПШОТОВ
// =============================================================================
void VisionSystemManager::setSaveSnapshots(bool save)
{
    m_saveSnapshots.store(save, std::memory_order_release);

    if (save) {
        emit visionLogMessage("Snapshot saving enabled - snapshots will be saved to /snaps directory");
        qDebug() << "Snapshot saving enabled";
    }
    else {
        emit visionLogMessage("Snapshot saving disabled");
        qDebug() << "Snapshot saving disabled";
    }
}

// =============================================================================
// НОВЫЙ МЕТОД ДЛЯ АСИНХРОННОГО СОХРАНЕНИЯ СНЭПШОТОВ
// =============================================================================
void VisionSystemManager::saveSnapshotAsync(const cv::Mat& frame, const QString& snapshotName)
{
    // =========================================================================
    // ПРЕОБРАЗОВАНИЕ OPENCV MAT В QIMAGE ДЛЯ БЕЗОПАСНОГО СОХРАНЕНИЯ
    // =========================================================================
    // Вместо работы с cv::Mat в отдельном потоке, преобразуем в QImage
    // который безопасно управляет памятью и может быть передан между потоками

    QImage image;
    try {
        // Проверяем валидность входного кадра
        if (frame.empty() || frame.cols == 0 || frame.rows == 0 || frame.data == nullptr) {
            qWarning() << "Invalid frame for snapshot:" << snapshotName;
            QMetaObject::invokeMethod(this, [this, snapshotName]() {
                emit visionLogMessage(QString("Invalid frame for snapshot %1").arg(snapshotName));
                });
            return;
        }

        // Преобразуем BGR OpenCV в RGB QImage
        cv::Mat rgbFrame;
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);

        // Создаем QImage из данных OpenCV
        image = QImage(rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
            rgbFrame.step, QImage::Format_RGB888).copy();

        // Проверяем, что преобразование прошло успешно
        if (image.isNull()) {
            qWarning() << "Failed to convert OpenCV frame to QImage for snapshot:" << snapshotName;
            QMetaObject::invokeMethod(this, [this, snapshotName]() {
                emit visionLogMessage(QString("Failed to convert frame for snapshot %1").arg(snapshotName));
                });
            return;
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Exception during frame conversion:" << e.what();
        QMetaObject::invokeMethod(this, [this, snapshotName, e]() {
            emit visionLogMessage(QString("Frame conversion error for %1: %2").arg(snapshotName).arg(e.what()));
            });
        return;
    }

    // =========================================================================
    // ГЕНЕРАЦИЯ УНИКАЛЬНОГО ИМЕНИ ФАЙЛА ДЛЯ КАЖДОГО СНЭПШОТА
    // =========================================================================
    QString uniqueSnapshotName = generateSnapshotName();
    qDebug() << "Generated snapshot name:" << uniqueSnapshotName;

    // =========================================================================
    // ИСПОЛЬЗОВАНИЕ STD::ASYNC ДЛЯ АСИНХРОННОГО СОХРАНЕНИЯ ЧЕРЕЗ QT
    // =========================================================================
    // Захватываем QImage по значению - он безопасно копируется между потоками
    auto saveTask = [this, image, uniqueSnapshotName]() {
        try {
            // =================================================================
            // СОЗДАНИЕ ДИРЕКТОРИИ SNAPS ЕСЛИ ОНА НЕ СУЩЕСТВУЕТ
            // =================================================================
            QDir snapsDir("snaps");
            if (!snapsDir.exists()) {
                if (snapsDir.mkpath(".")) {
                    qDebug() << "Created snaps directory";
                }
                else {
                    qWarning() << "Failed to create snaps directory";
                    QMetaObject::invokeMethod(this, [this, uniqueSnapshotName]() {
                        emit visionLogMessage(QString("Failed to create snaps directory for %1").arg(uniqueSnapshotName));
                        });
                    return;
                }
            }

            // =================================================================
            // ФОРМИРОВАНИЕ ПУТИ К ФАЙЛУ И СОХРАНЕНИЕ ЧЕРЕЗ QT
            // =================================================================
            QString filePath = snapsDir.filePath(uniqueSnapshotName + ".png");

            // Дополнительная проверка валидности QImage перед сохранением
            if (image.isNull() || image.width() == 0 || image.height() == 0) {
                qWarning() << "Invalid QImage for snapshot:" << uniqueSnapshotName;
                QMetaObject::invokeMethod(this, [this, uniqueSnapshotName]() {
                    emit visionLogMessage(QString("Invalid QImage for snapshot %1").arg(uniqueSnapshotName));
                    });
                return;
            }

            // Сохраняем изображение с помощью Qt - это безопаснее чем OpenCV в многопоточности
            bool saveResult = false;
            try {
                saveResult = image.save(filePath, "PNG");
            }
            catch (const std::exception& e) {
                qWarning() << "Qt exception during snapshot save:" << e.what();
                QMetaObject::invokeMethod(this, [this, uniqueSnapshotName, e]() {
                    emit visionLogMessage(QString("Qt error saving %1: %2").arg(uniqueSnapshotName).arg(e.what()));
                    });
                return;
            }

            if (saveResult) {
                qDebug() << "Snapshot saved:" << filePath;
                QMetaObject::invokeMethod(this, [this, uniqueSnapshotName]() {
                    emit visionLogMessage(QString("Snapshot saved: %1").arg(uniqueSnapshotName));
                    });
            }
            else {
                qWarning() << "Failed to save snapshot:" << filePath;
                QMetaObject::invokeMethod(this, [this, uniqueSnapshotName]() {
                    emit visionLogMessage(QString("Failed to save snapshot: %1").arg(uniqueSnapshotName));
                    });
            }
        }
        catch (const std::exception& e) {
            qWarning() << "Exception in snapshot saving:" << e.what();
            QMetaObject::invokeMethod(this, [this, uniqueSnapshotName, e]() {
                emit visionLogMessage(QString("Snapshot save error for %1: %2").arg(uniqueSnapshotName).arg(e.what()));
                });
        }
        };

    // Запускаем асинхронную задачу
    try {
        std::async(std::launch::async, saveTask);
    }
    catch (const std::exception& e) {
        qWarning() << "Failed to start async snapshot save:" << e.what();
        QMetaObject::invokeMethod(this, [this, uniqueSnapshotName]() {
            emit visionLogMessage(QString("Failed to start snapshot save for %1").arg(uniqueSnapshotName));
            });
    }
}

void VisionSystemManager::initializeVisionSystem()
{
    try {
        qDebug() << "Creating angle context...";

        // Создаем контекст обработки изображений
        m_angleContext = create_angle_context();
        if (!m_angleContext) {
            throw std::runtime_error("Failed to create angle context");
        }

        // Создаем контекст для callback-функции
        m_callbackContext = std::make_unique<CallbackContext>();
        m_callbackContext->angle_context = m_angleContext.get();
        m_callbackContext->stop = false;

        m_angleContext->save_snapshots = false;

        // Инициализируем камеру
        qDebug() << "Initializing camera...";
        m_cameraState = InitCamera();
        if (!m_cameraState) {
            throw std::runtime_error("Failed to initialize camera");
        }

        m_callbackContext->cameraState = m_cameraState;

        // Регистрируем callback-функцию
        int nRet = MV_CC_RegisterImageCallBackEx(m_cameraState->handle, ImageCallbackEx, m_callbackContext.get());
        if (nRet != MV_OK) {
            throw std::runtime_error("Failed to register image callback");
        }

        // Запускаем захват видео
        if (StartGrabbing(m_cameraState) != MV_OK) {
            throw std::runtime_error("Failed to start grabbing");
        }

        emit visionLogMessage("Vision system initialized successfully");
        qDebug() << "Vision system initialized successfully";

    }
    catch (const std::exception& e) {
        QString errorMsg = QString("Vision system initialization failed: %1").arg(e.what());
        qDebug() << errorMsg;

        // Очищаем ресурсы при ошибке инициализации
        cleanupVisionSystem();
        throw std::runtime_error(errorMsg.toStdString());
    }
}

void VisionSystemManager::cleanupVisionSystem()
{
    qDebug() << "Cleaning up vision system resources";

    // Останавливаем захват и деинициализируем камеру
    if (m_cameraState) {
        // Дерегистрируем callback-функцию
        if (m_cameraState->handle) {
            MV_CC_RegisterImageCallBackEx(m_cameraState->handle, NULL, NULL);
        }

        // Останавливаем захват если он активен
        if (m_cameraState->grabbingStarted) {
            StopGrabbing(m_cameraState);
        }

        // Деинициализируем камеру
        DeinitCamera(m_cameraState);
        m_cameraState = nullptr;
    }

    // Очистка ресурсов компьютерного зрения
    m_angleContext.reset();
    m_callbackContext.reset();

    qDebug() << "Vision system resources cleaned up";
}

void VisionSystemManager::visionMainLoop()
{
    emit visionLogMessage("Vision system main loop started");
    qDebug() << "Vision system main loop started";

    try {
        // Основной цикл обработки
        while (m_visionRunning) {
            cv::Mat frame_to_process;
            std::string snapshot_name;
            bool has_new_frame = false;

            {
                // Безопасный доступ к общим данным
                std::lock_guard<std::mutex> lock(m_dataMutex);

                if (m_angleContext && m_angleContext->new_frame_available) {
                    frame_to_process = m_angleContext->latest_frame.clone();
                    snapshot_name = m_angleContext->latest_snapshot_name;
                    m_angleContext->new_frame_available = false;
                    has_new_frame = true;
                    qDebug() << "New frame available for processing";
                }
            }

            // Обработка кадра
            if (has_new_frame && m_angleContext) {
                qDebug() << "Processing frame...";

                // =========================================================================
                // СОХРАНЕНИЕ СНЭПШОТА ЕСЛИ ВКЛЮЧЕНО
                // =========================================================================
                if (m_saveSnapshots.load(std::memory_order_acquire)) {
                    // Проверяем валидность кадра перед сохранением
                    if (!frame_to_process.empty() && frame_to_process.data != nullptr) {
                        // Используем нашу функцию для генерации уникального имени
                        QString qSnapshotName = generateSnapshotName();
                        qDebug() << "Saving snapshot:" << qSnapshotName;
                        saveSnapshotAsync(frame_to_process, qSnapshotName);
                    }
                    else {
                        qWarning() << "Invalid frame for snapshot saving - empty or null data";
                    }
                }

                if (process_frame(*m_angleContext, frame_to_process)) {
                    // Успешная обработка - отправляем результат
                    QString qSnapshotName = QString::fromStdString(snapshot_name);
                    int x_position = m_angleContext->x_position;
                    double total_time = m_angleContext->total_time;

                    // Используем invokeMethod для thread-safe вызова сигнала
                    QMetaObject::invokeMethod(this, [this, qSnapshotName, x_position, total_time]() {
                        emit visionResultReady(qSnapshotName, x_position, total_time);
                        });

                    // Логируем результат
                    QString logMessage = QString("Frame processed: X=%1, Time=%2ms")
                        .arg(x_position)
                        .arg(total_time, 0, 'f', 2);
                    emit visionLogMessage(logMessage);
                    qDebug() << logMessage;

                }
                else {
                    emit visionLogMessage("Frame processing failed");
                    qDebug() << "Frame processing failed!";

                    // Отправляем сообщение об ошибке
                    QString qSnapshotName = QString::fromStdString(snapshot_name);
                    QMetaObject::invokeMethod(this, [this, qSnapshotName]() {
                        emit visionResultReady(qSnapshotName, -1, 0.0);
                        });
                }
            }

            // Небольшая задержка для уменьшения нагрузки на CPU
            std::this_thread::sleep_for(10ms);

            // Проверяем флаг остановки
            if (!m_visionRunning) {
                break;
            }
        }

    }
    catch (const std::exception& e) {
        QString error = QString("Vision system error: %1").arg(e.what());
        qDebug() << error;
        QMetaObject::invokeMethod(this, [this, error]() {
            emit visionError(error);
            });
    }

    emit visionLogMessage("Vision system main loop ended");
    qDebug() << "Vision system main loop ended";
}