// =============================================================================
// ВКЛЮЧЕНИЕ БИБЛИОТЕК
// =============================================================================
#include "VisionSystemManager.h"
#include "angle.h"
#include "capture.h"

#include <QDebug>
#include <QMetaObject>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

VisionSystemManager::VisionSystemManager(QObject* parent)
    : QObject(parent)
    , m_visionRunning(false)
{
    qDebug() << "VisionSystemManager created";
}

VisionSystemManager::~VisionSystemManager()
{
    stopVisionSystem();
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

void VisionSystemManager::initializeVisionSystem()
{
    try {
        qDebug() << "Creating angle context...";

        // Создаем контекст обработки изображений
        m_angleContext = create_angle_context();
        if (!m_angleContext) {
            throw std::runtime_error("Failed to create angle context");
        }

        m_angleContext->save_snapshots = false;

        emit visionLogMessage("Vision system initialized successfully");
        qDebug() << "Vision system initialized successfully";

    }
    catch (const std::exception& e) {
        QString errorMsg = QString("Vision system initialization failed: %1").arg(e.what());
        qDebug() << errorMsg;
        throw std::runtime_error(errorMsg.toStdString());
    }
}

void VisionSystemManager::cleanupVisionSystem()
{
    qDebug() << "Cleaning up vision system resources";

    // Очистка ресурсов компьютерного зрения
    m_angleContext.reset();
    m_callbackContext.reset();
    m_cameraState.reset();

    qDebug() << "Vision system resources cleaned up";
}

void VisionSystemManager::visionMainLoop()
{
    emit visionLogMessage("Vision system main loop started");
    qDebug() << "Vision system main loop started";

    try {
        // Основной цикл обработки из main.cpp проекта компьютерного зрения
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
                if (process_frame(*m_angleContext, frame_to_process)) {
                    // Успешная обработка - отправляем результат
                    QString qSnapshotName = QString::fromStdString(snapshot_name);

                    // Используем invokeMethod для thread-safe вызова сигнала
                    QMetaObject::invokeMethod(this, [this, qSnapshotName]() {
                        emit visionResultReady(qSnapshotName,
                            m_angleContext->x_position,
                            m_angleContext->total_time);
                        });

                    // Логируем результат
                    QString logMessage = QString("Frame processed: X=%1, Time=%2ms")
                        .arg(m_angleContext->x_position)
                        .arg(m_angleContext->total_time, 0, 'f', 2);
                    emit visionLogMessage(logMessage);
                    qDebug() << logMessage;

                }
                else {
                    emit visionLogMessage("Frame processing failed");
                    qDebug() << "Frame processing failed";
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