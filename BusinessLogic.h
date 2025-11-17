// [file name]: BusinessLogic.h
#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QPixmap>
#include <QTimer>
#include <QPdfDocument>
#include <QDir>
#include <QThread>

#include "ImageLoader.h"

class VisionSystemManager;

class BusinessLogic : public QObject
{
    Q_OBJECT

public:
    explicit BusinessLogic(QObject* parent = nullptr);
    ~BusinessLogic();

public slots:
    // ==================== янйер ====================
    void connectToHost(const QString& ip, quint16 port);
    void disconnectFromHost();
    void sendMessage(const QString& message);

    // ==================== напюанрйю хгнапюфемхи ====================
    void loadImage(const QString& filePath);
    void processImage(const QPixmap& croppedImage, const QString& fileName);
    void clearImage();
    void setPdfRenderingDpi(int dpi);  //  якнр дкъ сярюмнбйх DPI

    // ==================== йнлоэчрепмне гпемхе ====================
    void startVisionSystem();
    void stopVisionSystem();
    void sendVisionResult(const QString& snapshotName, int xPosition, double totalTime);
    void setSaveSnapshots(bool save);

    // ==================== хмхжхюкхгюжхъ ====================
    void initialize();  // мнбши якнр дкъ хмхжхюкхгюжхх б опюбхкэмнл онрнйе

signals:
    // ==================== яхцмюкш дкъ GUI ====================
    void logMessage(const QString& message);
    void connectionStateChanged(bool connected);
    void imageLoaded(const QPixmap& preview, const QString& fileName);
    void imageProcessed();
    void imageCleared();
    void socketError(const QString& error);

    // ==================== яхцмюкш йнлоэчрепмнцн гпемхъ ====================
    void visionResultReceived(const QString& snapshotName, int xPosition, double totalTime);
    void visionResultReceivedForDisplay(const QString& displayMessage);
    void visionSystemError(const QString& error);
    void visionStatusChanged(bool running);

    // =============================================================================
    // мнбши яхцмюк дкъ оепедювх дюммшу нр янйерю б GUI
    // =============================================================================
    void socketDataReceived(const QString& data);

private slots:
    // ==================== бмсрпеммхе якнрш ====================
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();

private:
    // ==================== оепелеммше ====================
    QTcpSocket* m_tcpSocket;
    QPixmap m_currentImage;
    QString m_currentImagePath;

    // ==================== йнлоэчрепмне гпемхе ====================
    VisionSystemManager* m_visionManager;
    QThread* m_visionThread;
    int m_pdfRenderingDpi = 300;  // гмювемхе DPI он слнквюмхч

    // ==================== бяонлнцюрекэмше лерндш ====================
    void setupSocketConnections();
    QString getNextAvailableFilename();
};