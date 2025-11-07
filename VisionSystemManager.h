#pragma once

#include <QObject>
#include <QThread>
#include <atomic>
#include <memory>
#include <mutex>
#include <thread>

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

signals:
    void visionResultReady(const QString& snapshotName, int xPosition, double totalTime);
    void visionError(const QString& error);
    void visionLogMessage(const QString& message);
    void visionStatusChanged(bool running);

private:
    void visionMainLoop();
    void initializeVisionSystem();
    void cleanupVisionSystem();

    std::unique_ptr<AngleContext> m_angleContext;
    std::unique_ptr<CallbackContext> m_callbackContext;
    CameraState* m_cameraState;  // Изменено на сырой указатель

    std::atomic<bool> m_visionRunning{ false };
    std::thread m_visionThread;
    std::mutex m_dataMutex;
};