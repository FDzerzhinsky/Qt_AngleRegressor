#pragma once

#include <QObject>
#include <QTcpSocket>
#include <QPixmap>
#include <QTimer>
#include <QPdfDocument>

#include "ImageLoader.h"

class BusinessLogic : public QObject
{
    Q_OBJECT

public:
    explicit BusinessLogic(QObject* parent = nullptr);
    ~BusinessLogic();

public slots:
    // ==================== якнйерш ====================
    void connectToHost(const QString& ip, quint16 port);
    void disconnectFromHost();
    void sendMessage(const QString& message);

    // ==================== напюанрйю хгнапюфемхи ====================
    void loadImage(const QString& filePath);
    void processImage();
    void clearImage();

signals:
    // ==================== яхцмюкш дкъ GUI ====================
    void logMessage(const QString& message);
    void connectionStateChanged(bool connected);
    void imageLoaded(const QPixmap& preview, const QString& fileName);
    void imageProcessed();
    void imageCleared();
    void socketError(const QString& error);

private slots:
    // ==================== бмсрпеммхе якнрш ====================
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onProcessingFinished();

private:
    // ==================== оепелеммше ====================
    QTcpSocket* m_tcpSocket;
    QPixmap m_currentImage;
    QString m_currentImagePath;

    // ==================== бяонлнцюрекэмше лерндш ====================
    void setupSocketConnections();
};