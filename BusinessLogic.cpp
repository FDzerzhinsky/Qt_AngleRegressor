// [file name]: BusinessLogic.cpp
#include "BusinessLogic.h"
#include "VisionSystemManager.h"
#include <QFileInfo>
#include <QDir>
#include <QDebug>

BusinessLogic::BusinessLogic(QObject* parent)
    : QObject(parent)
    , m_tcpSocket(nullptr)  // ИНИЦИАЛИЗИРУЕМ НУЛЕВЫМ УКАЗАТЕЛЕМ
    , m_visionManager(new VisionSystemManager())
    , m_visionThread(new QThread(this))
{
    // НЕ создаем сокет здесь - он будет создан в initialize()
}

BusinessLogic::~BusinessLogic()
{
    // Останавливаем систему компьютерного зрения
    if (m_visionManager) {
        QMetaObject::invokeMethod(m_visionManager, "stopVisionSystem", Qt::BlockingQueuedConnection);
    }

    if (m_visionThread && m_visionThread->isRunning()) {
        m_visionThread->quit();
        m_visionThread->wait(1000);
    }

    if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->waitForDisconnected(1000);
    }

    // Удаляем сокет в правильном потоке
    if (m_tcpSocket) {
        m_tcpSocket->deleteLater();
    }
}

// =============================================================================
// НОВЫЙ МЕТОД ДЛЯ ИНИЦИАЛИЗАЦИИ В ПРАВИЛЬНОМ ПОТОКЕ
// =============================================================================
void BusinessLogic::initialize()
{
    // Создаем сокет в том же потоке, где работает BusinessLogic
    m_tcpSocket = new QTcpSocket(this);
    setupSocketConnections();

    // Инициализация системы компьютерного зрения
    m_visionManager->moveToThread(m_visionThread);

    // Подключаем сигналы системы компьютерного зрения
    connect(m_visionManager, &VisionSystemManager::visionResultReady,
        this, &BusinessLogic::visionResultReceived);
    connect(m_visionManager, &VisionSystemManager::visionError,
        this, &BusinessLogic::visionSystemError);
    connect(m_visionManager, &VisionSystemManager::visionLogMessage,
        this, &BusinessLogic::logMessage);
    connect(m_visionManager, &VisionSystemManager::visionStatusChanged,
        this, &BusinessLogic::visionStatusChanged);

    // Сигнал для отправки результатов через сокет
    connect(this, &BusinessLogic::visionResultReceived,
        this, &BusinessLogic::sendVisionResult);

    m_visionThread->start();

    emit logMessage("Business logic initialized");
    emit logMessage("Vision system ready");
}

// ==================== НАСТРОЙКА СОЕДИНЕНИЙ ====================

void BusinessLogic::setupSocketConnections()
{
    if (!m_tcpSocket) return;

    connect(m_tcpSocket, &QTcpSocket::connected, this, &BusinessLogic::onSocketConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &BusinessLogic::onSocketDisconnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &BusinessLogic::onSocketError);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &BusinessLogic::onSocketReadyRead);
}

// ==================== СЛОТЫ ДЛЯ РАБОТЫ С СОКЕТАМИ ====================

void BusinessLogic::connectToHost(const QString& ip, quint16 port)
{
    if (!m_tcpSocket) {
        emit logMessage("Error: Socket not initialized");
        return;
    }

    emit logMessage(QString("Connecting to %1:%2...").arg(ip).arg(port));
    m_tcpSocket->connectToHost(ip, port);
}

void BusinessLogic::disconnectFromHost()
{
    if (!m_tcpSocket) return;
    m_tcpSocket->disconnectFromHost();
}

void BusinessLogic::sendMessage(const QString& message)
{
    if (!m_tcpSocket) {
        emit logMessage("Error: Socket not initialized");
        return;
    }

    if (!message.isEmpty() && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray data = message.toUtf8() + '\n';
        qint64 bytesWritten = m_tcpSocket->write(data);
        if (bytesWritten == -1) {
            emit logMessage(QString("Failed to send message: %1").arg(m_tcpSocket->errorString()));
        }
        else {
            m_tcpSocket->flush(); // ОБЕСПЕЧИВАЕМ НЕМЕДЛЕННУЮ ОТПРАВКУ
            emit logMessage(QString("Sent: %1").arg(message));
        }
    }
    else if (!message.isEmpty()) {
        emit logMessage("Cannot send message - not connected to server");
    }
}

// ==================== СЛОТЫ ДЛЯ РАБОТЫ С ИЗОБРАЖЕНИЯМИ ====================

void BusinessLogic::loadImage(const QString& filePath)
{
    try {
        // Используем фабрику для создания подходящего загрузчика
        auto loader = ImageLoaderFactory::createLoader(filePath);
        QPixmap image = loader->load(filePath);

        if (!image.isNull()) {
            m_currentImage = image;
            m_currentImagePath = filePath;

            // Передаем оригинальное изображение (масштабирование теперь в GUI)
            emit imageLoaded(image, QFileInfo(filePath).fileName());
            emit logMessage("Image loaded: " + filePath);
        }
    }
    catch (const std::exception& e) {
        emit logMessage("Error loading image: " + QString(e.what()));
        emit socketError(QString(e.what()));
    }
}

void BusinessLogic::processImage(const QPixmap& croppedImage, const QString& fileName)
{
    if (croppedImage.isNull()) {
        emit logMessage("Error: No cropped image to process");
        return;
    }

    QString finalFileName = fileName;

    // Ensure filename has .png extension
    if (!finalFileName.toLower().endsWith(".png")) {
        finalFileName += ".png";
    }

    // Create netsurfaces directory if it doesn't exist
    QDir netsurfacesDir("netsurfaces");
    if (!netsurfacesDir.exists()) {
        netsurfacesDir.mkpath(".");
    }

    QString filePath = netsurfacesDir.filePath(finalFileName);

    // Save the cropped image
    if (croppedImage.save(filePath, "PNG")) {
        emit logMessage(QString("Image saved as: %1").arg(finalFileName));
        emit imageProcessed();
    }
    else {
        emit logMessage(QString("Failed to save image: %1").arg(finalFileName));
    }
}

void BusinessLogic::clearImage()
{
    m_currentImage = QPixmap();
    m_currentImagePath.clear();
    emit imageCleared();
    emit logMessage("Image cleared");
}

// ==================== КОМПЬЮТЕРНОЕ ЗРЕНИЕ ====================

void BusinessLogic::startVisionSystem()
{
    qDebug() << "BusinessLogic::startVisionSystem() - Sending start command to vision manager";

    if (!m_visionManager) {
        qDebug() << "BusinessLogic::startVisionSystem() - Vision manager is null!";
        emit logMessage("Error: Vision manager is not initialized");
        return;
    }

    if (!m_visionThread->isRunning()) {
        qDebug() << "BusinessLogic::startVisionSystem() - Vision thread is not running!";
        emit logMessage("Error: Vision thread is not running");
        return;
    }

    // ВАЖНО: Используем прямой вызов через QMetaObject::invokeMethod
    // с правильным указанием типа соединения
    bool result = QMetaObject::invokeMethod(m_visionManager, "startVisionSystem", Qt::QueuedConnection);
    qDebug() << "BusinessLogic::startVisionSystem() - Invoke method result:" << result;

    emit logMessage("Starting vision system...");
}

void BusinessLogic::stopVisionSystem()
{
    qDebug() << "BusinessLogic::stopVisionSystem() - Sending stop command to vision manager";
    QMetaObject::invokeMethod(m_visionManager, "stopVisionSystem", Qt::QueuedConnection);
    emit logMessage("Stopping vision system...");
}

void BusinessLogic::setSaveSnapshots(bool save)
{
    if (m_visionManager) {
        QMetaObject::invokeMethod(m_visionManager, "setSaveSnapshots", Qt::QueuedConnection, Q_ARG(bool, save));

        if (save) {
            emit logMessage("Snapshot saving enabled - snapshots will be saved to /snaps directory");
        }
        else {
            emit logMessage("Snapshot saving disabled");
        }
    }
}

// =============================================================================
// ПЕРЕРАБОТАННЫЙ МЕТОД ДЛЯ ОТПРАВКИ РЕЗУЛЬТАТОВ ОБРАБОТКИ
// =============================================================================
void BusinessLogic::sendVisionResult(const QString& snapshotName, int xPosition, double totalTime)
{
    // =========================================================================
    // ФОРМИРОВАНИЕ СООБЩЕНИЯ ДЛЯ ОТОБРАЖЕНИЯ В GUI
    // =========================================================================
    // Это сообщение отображается на вкладке "Определение угла"
    QString displayMessage = QString("[%1] X Position: %2, Processing Time: %3 ms")
        .arg(snapshotName)
        .arg(xPosition)
        .arg(totalTime, 0, 'f', 2);

    // Отправляем сообщение для отображения в GUI
    emit visionResultReceivedForDisplay(displayMessage);

    // =========================================================================
    // ФОРМИРОВАНИЕ СООБЩЕНИЯ ДЛЯ ОТПРАВКИ ПО СOKЕТУ
    // =========================================================================
    // Формируем сообщение в том же формате, что и в оригинальном проекте
    QString socketMessage = QString("%1;Position:%2;Time:%3 ms")
        .arg(snapshotName)
        .arg(xPosition)
        .arg(totalTime, 0, 'f', 2);

    // Отправляем через существующий сокет (если он открыт)
    sendMessage(socketMessage);
}

// ==================== ОБРАБОТЧИКИ СОБЫТИЙ СОКЕТА ====================

void BusinessLogic::onSocketConnected()
{
    emit logMessage("Connected to server");
    emit connectionStateChanged(true);
}

void BusinessLogic::onSocketDisconnected()
{
    emit logMessage("Disconnected from server");
    emit connectionStateChanged(false);
}

void BusinessLogic::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
        if (m_tcpSocket) {
            QString errorString = m_tcpSocket->errorString();
            emit logMessage("Socket error: " + errorString);
            emit socketError(errorString);
        }
}

void BusinessLogic::onSocketReadyRead()
{
    if (!m_tcpSocket) return;

    QByteArray data = m_tcpSocket->readAll();
    QString message = QString::fromUtf8(data).trimmed();
    emit logMessage(QString("Received: %1").arg(message));
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================

QString BusinessLogic::getNextAvailableFilename()
{
    QDir netsurfacesDir("netsurfaces");

    // Create directory if it doesn't exist
    if (!netsurfacesDir.exists()) {
        netsurfacesDir.mkpath(".");
    }

    // Find the next available number
    int nextNumber = 1;
    while (netsurfacesDir.exists(QString("netsurface%1.png").arg(nextNumber))) {
        nextNumber++;
    }

    return QString("netsurface%1.png").arg(nextNumber);
}