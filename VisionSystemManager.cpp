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
}

VisionSystemManager::~VisionSystemManager()
{
    stopVisionSystem();
}

void VisionSystemManager::startVisionSystem()
{
    if (m_visionRunning) {
        emit visionLogMessage("Vision system is already running");
        return;
    }

    try {
        emit visionLogMessage("Initializing vision system...");

        // Инициализируем систему компьютерного зрения
        initializeVisionSystem();

        m_visionRunning = true;

        // Запускаем основной цикл в отдельном std::thread
        m_visionThread = std::thread(&VisionSystemManager::visionMainLoop, this);

        emit visionStatusChanged(true);
        emit visionLogMessage("Vision system started successfully");

    }
    catch (const std::exception& e) {
        emit visionError(QString("Failed to start vision system: %1").arg(e.what()));
    }
}

void VisionSystemManager::stopVisionSystem()
{
    if (!m_visionRunning) {
        return;
    }

    emit visionLogMessage("Stopping vision system...");
    m_visionRunning = false;

    if (m_visionThread.joinable()) {
        m_visionThread.join();
    }

    cleanupVisionSystem();

    emit visionStatusChanged(false);
    emit visionLogMessage("Vision system stopped");
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
        // Создаем контекст обработки изображений
        m_angleContext = create_angle_context();
        m_angleContext->save_snapshots = false;

        emit visionLogMessage("Vision system initialized successfully");

    }
    catch (const std::exception& e) {
        throw std::runtime_error(std::string("Vision system initialization failed: ") + e.what());
    }
}

void VisionSystemManager::cleanupVisionSystem()
{
    // Очистка ресурсов компьютерного зрения
    m_angleContext.reset();
    m_callbackContext.reset();
    m_cameraState.reset();
}

void VisionSystemManager::visionMainLoop()
{
    emit visionLogMessage("Vision system main loop started");

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
                }
            }

            // Обработка кадра
            if (has_new_frame && m_angleContext) {
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

                }
                else {
                    emit visionLogMessage("Frame processing failed");
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
        QMetaObject::invokeMethod(this, [this, error]() {
            emit visionError(error);
            });
    }

    emit visionLogMessage("Vision system main loop ended");
}