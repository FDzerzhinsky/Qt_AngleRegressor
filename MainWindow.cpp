#include "MainWindow.h"
#include "ui_MainWindow.h"
#include <QFileInfo>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , tcpSocket(new QTcpSocket(this))
{
    ui->setupUi(this);

    // ==================== НАСТРОЙКА НАЧАЛЬНОГО СОСТОЯНИЯ ====================

    // Настройка текстовых подсказок
    ui->messageLineEdit->setPlaceholderText("Enter message to send...");

    // ==================== СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "СОКЕТ" ====================

    // Управление подключением
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);

    // Обработка событий сокета
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onSocketConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onSocketDisconnected);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onSocketError);
    connect(tcpSocket, &QTcpSocket::readyRead, this, &MainWindow::onSocketReadyRead);

    // Элементы управления логом
    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendMessageClicked);
    connect(ui->messageLineEdit, &QLineEdit::textChanged, this, &MainWindow::onMessageTextChanged);

    // ==================== СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "РАЗВЁРТКА" ====================

    connect(ui->loadImageButton, &QPushButton::clicked, this, &MainWindow::onLoadImageClicked);
    connect(ui->processImageButton, &QPushButton::clicked, this, &MainWindow::onProcessImageClicked);
    connect(ui->clearImageButton, &QPushButton::clicked, this, &MainWindow::onClearImageClicked);

    // ==================== ИНИЦИАЛИЗАЦИЯ СОСТОЯНИЯ ====================

    updateSendButtonState();
    updateImageButtonsState();

    logMessage("Vision System GUI initialized");
    logMessage("Ready to work with sockets and image processing");
}

MainWindow::~MainWindow()
{
    delete ui;
}

// ==================== РЕАЛИЗАЦИЯ СЛОТОВ ДЛЯ ВКЛАДКИ "СОКЕТ" ====================

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

// ==================== РЕАЛИЗАЦИЯ СЛОТОВ ДЛЯ ВКЛАДКИ "РАЗВЁРТКА" ====================

void MainWindow::onLoadImageClicked()
{
    // Открываем диалог выбора файла
    QString fileName = QFileDialog::getOpenFileName(this,
        "Select Image",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp *.tiff);;All files (*.*)");

    if (!fileName.isEmpty()) {
        QPixmap image(fileName);
        if (!image.isNull()) {
            currentImage = image;
            currentImagePath = fileName;

            // Масштабируем изображение для превью с сохранением пропорций
            QPixmap preview = image.scaled(ui->imagePreviewLabel->width() - 20,
                ui->imagePreviewLabel->height() - 20,
                Qt::KeepAspectRatio,
                Qt::SmoothTransformation);

            // Устанавливаем превью и обновляем интерфейс
            ui->imagePreviewLabel->setPixmap(preview);
            ui->imagePathLabel->setText(QFileInfo(fileName).fileName());

            // Обновляем состояние кнопок и логируем событие
            updateImageButtonsState();
            logMessage("Image loaded: " + fileName);

            // Переключаемся на вкладку с изображением для удобства пользователя
            ui->tabWidget->setCurrentIndex(1);
        }
        else {
            // Обработка ошибки загрузки изображения
            logMessage("Error loading image: " + fileName);
            QMessageBox::warning(this, "Error", "Failed to load image");
        }
    }
}

void MainWindow::onProcessImageClicked()
{
    if (!currentImage.isNull()) {
        logMessage("Started image processing: " + currentImagePath);

        // Временно отключаем кнопку обработки, но не меняем её текст
        ui->processImageButton->setEnabled(false);

        // Имитация длительной обработки изображения
        // В реальном приложении здесь будет ваш алгоритм обработки
        QTimer::singleShot(2000, this, [this]() {
            logMessage("Image processing completed");

            // Восстанавливаем кнопку в исходное состояние
            ui->processImageButton->setEnabled(true);

            // Переключаемся на вкладку результатов
            ui->tabWidget->setCurrentIndex(2);
            logMessage("Results ready on Results tab");

            // Здесь можно добавить логику отображения результатов обработки
            // Например, вывод углов, статистики и т.д.
            });
    }
}

void MainWindow::onClearImageClicked()
{
    // Очищаем текущее изображение и связанные данные
    currentImage = QPixmap();
    currentImagePath.clear();

    // Сбрасываем превью и текстовые метки
    ui->imagePreviewLabel->clear();
    ui->imagePreviewLabel->setText("Image Preview");
    ui->imagePathLabel->setText("No file selected");

    // Обновляем состояние кнопок и логируем действие
    updateImageButtonsState();
    logMessage("Image cleared");
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================

void MainWindow::logMessage(const QString& message)
{
    // Добавляем сообщение в лог с временной меткой
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::updateSendButtonState()
{
    // Обновляем состояние кнопки отправки в зависимости от подключения и наличия текста
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = (tcpSocket->state() == QAbstractSocket::ConnectedState);

    ui->sendMessageButton->setEnabled(hasText && isConnected);
}

void MainWindow::updateImageButtonsState()
{
    // Обновляем состояние кнопок в зависимости от наличия загруженного изображения
    bool hasImage = !currentImage.isNull();
    ui->processImageButton->setEnabled(hasImage);
    ui->clearImageButton->setEnabled(hasImage);
}