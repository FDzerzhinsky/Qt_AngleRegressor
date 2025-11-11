// [file name]: VisionSystemManager.h
#pragma once

#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

// =============================================================================
// бйкчвемхе ахакхнрей OPENCV
// =============================================================================
#include <opencv2/opencv.hpp>

struct AngleContext;
struct CallbackContext;
struct CameraState;

class VisionSystemManager : public QObject
{
    Q_OBJECT

public:
    explicit VisionSystemManager(QObject* parent = nullptr);
    ~VisionSystemManager();

public slots:
    void startVisionSystem();
    void stopVisionSystem();
    void configureVisionSystem(bool saveSnapshots = false);
    void setSaveSnapshots(bool save);  // мнбши якнр дкъ сопюбкемхъ янупюмемхел ямщоьнрнб

signals:
    void visionResultReady(const QString& snapshotName, int xPosition, double totalTime);
    void visionError(const QString& error);
    void visionLogMessage(const QString& message);
    void visionStatusChanged(bool running);

private:
    void visionMainLoop();
    void initializeVisionSystem();
    void cleanupVisionSystem();
    void saveSnapshotAsync(const cv::Mat& frame, const QString& snapshotName);  // мнбши лернд дкъ юяхмупнммнцн янупюмемхъ
    QString generateSnapshotName();  // мнбюъ тсмйжхъ дкъ цемепюжхх хлемх тюикю

    std::unique_ptr<AngleContext> m_angleContext;
    std::unique_ptr<CallbackContext> m_callbackContext;
    CameraState* m_cameraState;

    std::atomic<bool> m_visionRunning{ false };
    std::atomic<bool> m_saveSnapshots{ false };  // юрнлюпмши ткюц дкъ янупюмемхъ ямщоьнрнб
    std::thread m_visionThread;
    std::mutex m_dataMutex;
};