// [file name]: VisionSystemManager.cpp
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
    if (m_visionThread.joinable()) {
        m_visionThread.join();
    }
}

QString VisionSystemManager::generateSnapshotName()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    QString timestamp = currentTime.toString("yyyy-MM-dd_hh-mm-ss");
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

        initializeVisionSystem();

        m_visionRunning = true;
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

void VisionSystemManager::setSaveSnapshots(bool save)
{
    m_saveSnapshots.store(save, std::memory_order_release);

    if (save) {
        emit visionLogMessage("Snapshot saving enabled - snapshots will be saved to /snaps directory");
        qDebug() << "Snapshot saving enabled";

        // —ÓÁ‰‡ÂÏ Ô‡ÔÍÛ snaps ÂÒÎË Â∏ ÌÂÚ
        QDir snapsDir("snaps");
        if (!snapsDir.exists()) {
            snapsDir.mkpath(".");
        }
    }
    else {
        emit visionLogMessage("Snapshot saving disabled");
        qDebug() << "Snapshot saving disabled";
    }
}

void VisionSystemManager::saveSnapshotAsync(const cv::Mat& frame, const QString& snapshotName)
{
    if (frame.empty() || frame.cols == 0 || frame.rows == 0 || frame.data == nullptr) {
        qWarning() << "Invalid frame for snapshot:" << snapshotName;
        return;
    }

    QImage image;
    try {
        cv::Mat rgbFrame;
        cv::cvtColor(frame, rgbFrame, cv::COLOR_BGR2RGB);
        image = QImage(rgbFrame.data, rgbFrame.cols, rgbFrame.rows,
            rgbFrame.step, QImage::Format_RGB888).copy();

        if (image.isNull()) {
            qWarning() << "Failed to convert OpenCV frame to QImage for snapshot:" << snapshotName;
            return;
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Exception during frame conversion:" << e.what();
        return;
    }

    auto saveTask = [this, image, snapshotName]() {
        try {
            QDir snapsDir("snaps");
            if (!snapsDir.exists()) {
                if (!snapsDir.mkpath(".")) {
                    qWarning() << "Failed to create snaps directory";
                    return;
                }
            }

            QString filePath = snapsDir.filePath(snapshotName + ".png");

            if (image.isNull() || image.width() == 0 || image.height() == 0) {
                qWarning() << "Invalid QImage for snapshot:" << snapshotName;
                return;
            }

            bool saveResult = image.save(filePath, "PNG");
            if (saveResult) {
                qDebug() << "Snapshot saved:" << filePath;
                QMetaObject::invokeMethod(this, [this, snapshotName]() {
                    emit visionLogMessage(QString("Snapshot saved: %1").arg(snapshotName));
                    });
            }
            else {
                qWarning() << "Failed to save snapshot:" << filePath;
            }
        }
        catch (const std::exception& e) {
            qWarning() << "Exception in snapshot saving:" << e.what();
        }
        };

    try {
        std::async(std::launch::async, saveTask);
    }
    catch (const std::exception& e) {
        qWarning() << "Failed to start async snapshot save:" << e.what();
    }
}

void VisionSystemManager::initializeVisionSystem()
{
    try {
        qDebug() << "Creating angle context...";
        m_angleContext = create_angle_context();
        if (!m_angleContext) {
            throw std::runtime_error("Failed to create angle context");
        }

        m_callbackContext = std::make_unique<CallbackContext>();
        m_callbackContext->angle_context = m_angleContext.get();
        m_callbackContext->stop = false;
        m_angleContext->save_snapshots = false;

        qDebug() << "Initializing camera...";
        m_cameraState = InitCamera();
        if (!m_cameraState) {
            throw std::runtime_error("Failed to initialize camera");
        }

        m_callbackContext->cameraState = m_cameraState;

        int nRet = MV_CC_RegisterImageCallBackEx(m_cameraState->handle, ImageCallbackEx, m_callbackContext.get());
        if (nRet != MV_OK) {
            throw std::runtime_error("Failed to register image callback");
        }

        if (StartGrabbing(m_cameraState) != MV_OK) {
            throw std::runtime_error("Failed to start grabbing");
        }

        emit visionLogMessage("Vision system initialized successfully");
        qDebug() << "Vision system initialized successfully";

    }
    catch (const std::exception& e) {
        QString errorMsg = QString("Vision system initialization failed: %1").arg(e.what());
        qDebug() << errorMsg;
        cleanupVisionSystem();
        throw std::runtime_error(errorMsg.toStdString());
    }
}

void VisionSystemManager::cleanupVisionSystem()
{
    qDebug() << "Cleaning up vision system resources";

    if (m_cameraState) {
        if (m_cameraState->handle) {
            MV_CC_RegisterImageCallBackEx(m_cameraState->handle, NULL, NULL);
        }

        if (m_cameraState->grabbingStarted) {
            StopGrabbing(m_cameraState);
        }

        DeinitCamera(m_cameraState);
        m_cameraState = nullptr;
    }

    m_angleContext.reset();
    m_callbackContext.reset();

    qDebug() << "Vision system resources cleaned up";
}

void VisionSystemManager::visionMainLoop()
{
    emit visionLogMessage("Vision system main loop started");
    qDebug() << "Vision system main loop started";

    try {
        while (m_visionRunning) {
            cv::Mat frame_to_process;
            bool has_new_frame = false;

            {
                std::lock_guard<std::mutex> lock(m_dataMutex);
                if (m_angleContext && m_angleContext->new_frame_available) {
                    frame_to_process = m_angleContext->latest_frame.clone();
                    m_angleContext->new_frame_available = false;
                    has_new_frame = true;
                    qDebug() << "New frame available for processing";
                }
            }

            if (has_new_frame && m_angleContext) {
                qDebug() << "Processing frame...";

                // =========================================================================
                // —Œ’–¿Õ≈Õ»≈ —Õ›œÿŒ“¿ ≈—À» ¬ Àﬁ◊≈ÕŒ
                // =========================================================================
                if (m_saveSnapshots.load(std::memory_order_acquire)) {
                    if (!frame_to_process.empty() && frame_to_process.data != nullptr) {
                        QString qSnapshotName = generateSnapshotName();
                        qDebug() << "Saving snapshot:" << qSnapshotName;
                        saveSnapshotAsync(frame_to_process, qSnapshotName);
                    }
                    else {
                        qWarning() << "Invalid frame for snapshot saving";
                    }
                }

                // Œ·‡·‡Ú˚‚‡ÂÏ Í‡‰
                if (process_frame(*m_angleContext, frame_to_process)) {
                    // ”—œ≈ÿÕ¿ﬂ Œ¡–¿¡Œ“ ¿ - Œ“œ–¿¬Àﬂ≈Ã –≈«”À‹“¿“ ¡≈« »Ã≈Õ» —Õ›œÿŒ“¿
                    int x_position = m_angleContext->x_position;
                    double total_time = m_angleContext->total_time;

                    // Œ“œ–¿¬Àﬂ≈Ã œ”—“Œ≈ »Ãﬂ —Õ›œÿŒ“¿, “¿   ¿  ŒÕŒ √≈Õ≈–»–”≈“—ﬂ ¬ VisionSystemManager
                    QMetaObject::invokeMethod(this, [this, x_position, total_time]() {
                        emit visionResultReady("", x_position, total_time);
                        });

                    QString logMessage = QString("Frame processed: X=%1, Time=%2ms")
                        .arg(x_position)
                        .arg(total_time, 0, 'f', 2);
                    emit visionLogMessage(logMessage);
                    qDebug() << logMessage;

                }
                else {
                    QString errorMessage = "Frame processing failed";
                    emit visionLogMessage(errorMessage);
                    qDebug() << errorMessage;

                    QMetaObject::invokeMethod(this, [this]() {
                        emit visionResultReady("", -1, 0.0);
                        });
                }
            }

            std::this_thread::sleep_for(10ms);

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