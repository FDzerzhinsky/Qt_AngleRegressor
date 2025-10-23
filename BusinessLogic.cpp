#include "BusinessLogic.h"
#include <QFileInfo>
#include <QDir>

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

// ==================== ÍÀÑÒĞÎÉÊÀ ÑÎÅÄÈÍÅÍÈÉ ====================

void BusinessLogic::setupSocketConnections()
{
    connect(m_tcpSocket, &QTcpSocket::connected, this, &BusinessLogic::onSocketConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &BusinessLogic::onSocketDisconnected);
    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, &BusinessLogic::onSocketError);
    connect(m_tcpSocket, &QTcpSocket::readyRead, this, &BusinessLogic::onSocketReadyRead);
}

// ==================== ÑËÎÒÛ ÄËß ĞÀÁÎÒÛ Ñ ÑÎÊÅÒÀÌÈ ====================

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

// ==================== ÑËÎÒÛ ÄËß ĞÀÁÎÒÛ Ñ ÈÇÎÁĞÀÆÅÍÈßÌÈ ====================

void BusinessLogic::loadImage(const QString& filePath)
{
    try {
        // Èñïîëüçóåì ôàáğèêó äëÿ ñîçäàíèÿ ïîäõîäÿùåãî çàãğóç÷èêà
        auto loader = ImageLoaderFactory::createLoader(filePath);
        QPixmap image = loader->load(filePath);

        if (!image.isNull()) {
            m_currentImage = image;
            m_currentImagePath = filePath;

            // Ïåğåäàåì îğèãèíàëüíîå èçîáğàæåíèå (ìàñøòàáèğîâàíèå òåïåğü â GUI)
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

// ==================== ÎÁĞÀÁÎÒ×ÈÊÈ ÑÎÁÛÒÈÉ ÑÎÊÅÒÀ ====================

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

// ==================== ÂÑÏÎÌÎÃÀÒÅËÜÍÛÅ ÌÅÒÎÄÛ ====================

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