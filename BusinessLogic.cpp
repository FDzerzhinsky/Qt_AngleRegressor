#include "BusinessLogic.h"
#include <QFileInfo>

BusinessLogic::BusinessLogic(QObject* parent)
    : QObject(parent)
    , m_tcpSocket(new QTcpSocket(this))
{
    setupSocketConnections();
    emit logMessage("Business logic initialized");
}

BusinessLogic::~BusinessLogic()
{
    if (m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        m_tcpSocket->disconnectFromHost();
        m_tcpSocket->waitForDisconnected(1000);
    }
}

// ==================== НАСТРОЙКА СОЕДИНЕНИЙ ====================

void BusinessLogic::setupSocketConnections()
{
    connect(m_tcpSocket, &QTcpSocket::connected, this, &BusinessLogic::onSocketConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &BusinessLogic::onSocketDisconnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &BusinessLogic::onSocketError);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &BusinessLogic::onSocketReadyRead);
}

// ==================== СЛОТЫ ДЛЯ РАБОТЫ С СОКЕТАМИ ====================

void BusinessLogic::connectToHost(const QString& ip, quint16 port)
{
    emit logMessage(QString("Connecting to %1:%2...").arg(ip).arg(port));
    m_tcpSocket->connectToHost(ip, port);
}

void BusinessLogic::disconnectFromHost()
{
    m_tcpSocket->disconnectFromHost();
}

void BusinessLogic::sendMessage(const QString& message)
{
    if (!message.isEmpty() && m_tcpSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray data = message.toUtf8() + '\n';
        m_tcpSocket->write(data);
        emit logMessage(QString("Sent: %1").arg(message));
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

void BusinessLogic::processImage()
{
    if (!m_currentImage.isNull()) {
        emit logMessage("Started image processing: " + m_currentImagePath);

        // Имитация длительной обработки в отдельном потоке
        QTimer::singleShot(2000, this, &BusinessLogic::onProcessingFinished);
    }
}

void BusinessLogic::clearImage()
{
    m_currentImage = QPixmap();
    m_currentImagePath.clear();
    emit imageCleared();
    emit logMessage("Image cleared");
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
        QString errorString = m_tcpSocket->errorString();
    emit logMessage("Socket error: " + errorString);
    emit socketError(errorString);
}

void BusinessLogic::onSocketReadyRead()
{
    QByteArray data = m_tcpSocket->readAll();
    QString message = QString::fromUtf8(data).trimmed();
    emit logMessage(QString("Received: %1").arg(message));
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================

void BusinessLogic::onProcessingFinished()
{
    emit logMessage("Image processing completed");
    emit imageProcessed();
}