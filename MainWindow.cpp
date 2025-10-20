#include "MainWindow.h"
#include "ui_MainWindow.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpSocket(new QTcpSocket(this))
{
    ui->setupUi(this);

    // Настройка начального состояния
    ui->messageLineEdit->setPlaceholderText("Enter message to send...");

    // Соединения для управления подключением
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    // Соединения для сокета
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onSocketError);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);

    // Соединения для добавленных элементов
    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendMessageClicked);
    connect(ui->messageLineEdit, &QLineEdit::textChanged, this, &MainWindow::onMessageTextChanged);

    // Начальное состояние кнопки отправки
    updateSendButtonState();

    logMessage("Vision System GUI initialized");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onConnectClicked()
{
    QString ip = ui->ipLineEdit->text();
    quint16 port = static_cast<quint16>(ui->portSpinBox->value());

    logMessage(QString("Connecting to %1:%2...").arg(ip).arg(port));
    tcpSocket->connectToHost(ip, port);
}

void MainWindow::onDisconnectClicked()
{
    tcpSocket->disconnectFromHost();
}

void MainWindow::onSocketConnected()
{
    logMessage("Connected to server");
    ui->connectButton->setEnabled(false);
    ui->disconnectButton->setEnabled(true);
    updateSendButtonState();
}

void MainWindow::onSocketDisconnected()
{
    logMessage("Disconnected from server");
    ui->connectButton->setEnabled(true);
    ui->disconnectButton->setEnabled(false);
    updateSendButtonState();
}

void MainWindow::onSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
        logMessage(QString("Socket error: %1").arg(tcpSocket->errorString()));
}

void MainWindow::onSocketReadyRead()
{
    QByteArray data = tcpSocket->readAll();
    QString message = QString::fromUtf8(data).trimmed();
    logMessage(QString("Received: %1").arg(message));
}

void MainWindow::onClearLogClicked()
{
    ui->logTextEdit->clear();
    logMessage("Log cleared");
}

void MainWindow::onSendMessageClicked()
{
    QString message = ui->messageLineEdit->text().trimmed();

    if (!message.isEmpty() && tcpSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray data = message.toUtf8() + '\n';
        tcpSocket->write(data);

        logMessage(QString("Sent: %1").arg(message));
        ui->messageLineEdit->clear();
    }
    else if (!message.isEmpty()) {
        logMessage("Cannot send message - not connected to server");
    }
}

void MainWindow::onMessageTextChanged(const QString& text)
{
    Q_UNUSED(text)
        updateSendButtonState();
}

void MainWindow::logMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::updateSendButtonState()
{
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = (tcpSocket->state() == QAbstractSocket::ConnectedState);

    ui->sendMessageButton->setEnabled(hasText && isConnected);
}