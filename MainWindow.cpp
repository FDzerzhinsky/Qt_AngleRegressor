#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "BusinessLogic.h"
#include "CropDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_businessLogic(new BusinessLogic)
    , m_businessThread(new QThread(this))
    , m_cropDialog(new CropDialog(this))
{
    ui->setupUi(this);

    // ==================== НАСТРОЙКА НАЧАЛЬНОГО СОСТОЯНИЯ ====================
    ui->messageLineEdit->setPlaceholderText("Enter message to send...");

    // ==================== НАСТРОЙКА ПОТОКА ДЛЯ БИЗНЕС-ЛОГИКИ ====================
    // Перемещаем бизнес-логику в отдельный поток для предотвращения блокировки GUI
    m_businessLogic->moveToThread(m_businessThread);
    m_businessThread->start();

    // ==================== НАСТРОЙКА СОЕДИНЕНИЙ СИГНАЛОВ И СЛОТОВ ====================
    setupConnections();

    // ==================== ИНИЦИАЛИЗАЦИЯ СОСТОЯНИЯ ИНТЕРФЕЙСА ====================
    updateSendButtonState();
    updateImageButtonsState();
}

MainWindow::~MainWindow()
{
    // ==================== КОРРЕКТНОЕ ЗАВЕРШЕНИЕ ПОТОКА ====================
    // Плавное завершение работы потока бизнес-логики
    m_businessThread->quit();
    m_businessThread->wait(1000);

    // Принудительное завершение если поток не ответил
    if (m_businessThread->isRunning()) {
        m_businessThread->terminate();
        m_businessThread->wait();
    }

    delete m_businessLogic;
    delete ui;
}

// ==================== НАСТРОЙКА СОЕДИНЕНИЙ МЕЖДУ GUI И БИЗНЕС-ЛОГИКОЙ ====================
void MainWindow::setupConnections()
{
    // ==================== СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "СОКЕТ" ====================
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendMessageClicked);
    connect(ui->messageLineEdit, &QLineEdit::textChanged, this, &MainWindow::onMessageTextChanged);

    // ==================== СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "РАЗВЁРТКА" ====================
    connect(ui->loadImageButton, &QPushButton::clicked, this, &MainWindow::onLoadImageClicked);
    connect(ui->processImageButton, &QPushButton::clicked, this, &MainWindow::onProcessImageClicked);
    connect(ui->clearImageButton, &QPushButton::clicked, this, &MainWindow::onClearImageClicked);

    // ==================== СОЕДИНЕНИЯ С БИЗНЕС-ЛОГИКОЙ ====================
    // Сигналы от бизнес-логики к GUI (межпоточные соединения)
    connect(m_businessLogic, &BusinessLogic::logMessage, this, &MainWindow::onLogMessage);
    connect(m_businessLogic, &BusinessLogic::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_businessLogic, &BusinessLogic::imageLoaded, this, &MainWindow::onImageLoaded);
    connect(m_businessLogic, &BusinessLogic::imageProcessed, this, &MainWindow::onImageProcessed);
    connect(m_businessLogic, &BusinessLogic::imageCleared, this, &MainWindow::onImageCleared);
    connect(m_businessLogic, &BusinessLogic::socketError, this, &MainWindow::onSocketError);
}

// ==================== РЕАЛИЗАЦИЯ СЛОТОВ ДЛЯ ВКЛАДКИ "СОКЕТ" ====================
void MainWindow::onConnectClicked()
{
    QString ip = ui->ipLineEdit->text();
    quint16 port = static_cast<quint16>(ui->portSpinBox->value());
    m_businessLogic->connectToHost(ip, port);
}

void MainWindow::onDisconnectClicked()
{
    m_businessLogic->disconnectFromHost();
}

void MainWindow::onClearLogClicked()
{
    ui->logTextEdit->clear();
    onLogMessage("Log cleared");
}

void MainWindow::onSendMessageClicked()
{
    QString message = ui->messageLineEdit->text().trimmed();
    m_businessLogic->sendMessage(message);
    ui->messageLineEdit->clear();
}

void MainWindow::onMessageTextChanged(const QString& text)
{
    Q_UNUSED(text)
        updateSendButtonState();
}

// ==================== РЕАЛИЗАЦИЯ СЛОТОВ ДЛЯ ВКЛАДКИ "РАЗВЁРТКА" ====================
void MainWindow::onLoadImageClicked()
{
    // Диалог выбора файла с поддержкой различных форматов
    QString fileName = QFileDialog::getOpenFileName(this,
        "Select Image",
        "",
        "Images (*.png *.jpg *.jpeg *.bmp *.tiff);;PDF files (*.pdf);;All files (*.*)");

    if (!fileName.isEmpty()) {
        m_businessLogic->loadImage(fileName);
    }
}

void MainWindow::onProcessImageClicked()
{
    if (!m_croppedImage.isNull()) {
        ui->processImageButton->setEnabled(false);

        QString filename = ui->netnamelineEdit->text().trimmed();

        // Передаем обрезанное изображение и имя файла в бизнес-логику для сохранения
        m_businessLogic->processImage(m_croppedImage, filename);
    }
    else {
        QMessageBox::warning(this, "Предупреждение", "Сначала загрузите и обрежьте изображение");
    }
}

void MainWindow::onClearImageClicked()
{
    m_businessLogic->clearImage();
    m_currentImage = QPixmap();
    m_croppedImage = QPixmap();
}

// ==================== СЛОТЫ ДЛЯ ОБРАБОТКИ СИГНАЛОВ ОТ БИЗНЕС-ЛОГИКИ ====================
void MainWindow::onLogMessage(const QString& message)
{
    // Добавление временной метки к каждому сообщению в логе
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    // Обновление состояния кнопок подключения/отключения
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    updateSendButtonState();
}

void MainWindow::onImageLoaded(const QPixmap& originalImage, const QString& fileName)
{
    m_currentImage = originalImage;

    // Показываем диалог кадрирования перед отображением изображения
    m_cropDialog->setImage(originalImage);

    if (m_cropDialog->exec() == QDialog::Accepted) {
        // Получаем кадрированное изображение
        m_croppedImage = m_cropDialog->getCroppedImage();

        // Масштабируем изображение для отображения в preview
        QSize labelSize = ui->imagePreviewLabel->size();
        QPixmap scaledImage = m_croppedImage.scaled(
            labelSize.width() - 10,
            labelSize.height() - 10,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );

        // Устанавливаем изображение и информацию о файле
        ui->imagePreviewLabel->setPixmap(scaledImage);

        // Добавляем пометку о кадрировании если оно было применено
        if (m_cropDialog->getCroppedImage().size() != originalImage.size()) {
            ui->imagePathLabel->setText(fileName + " (cropped)");
        }
        else {
            ui->imagePathLabel->setText(fileName);
        }

        // Обновляем поле имени файла с автоматически сгенерированным именем
        updateNetNameEdit();

        updateImageButtonsState();
        ui->tabWidget->setCurrentIndex(1); // Переключаемся на вкладку с изображением
    }
}

void MainWindow::onImageProcessed()
{
    ui->processImageButton->setEnabled(true);
    ui->tabWidget->setCurrentIndex(2); // Переключаемся на вкладку "Результаты"
}

void MainWindow::onImageCleared()
{
    ui->imagePreviewLabel->clear();
    ui->imagePreviewLabel->setText("Image Preview");
    ui->imagePathLabel->setText("No file selected");
    ui->netnamelineEdit->clear();
    updateImageButtonsState();
}

void MainWindow::onSocketError(const QString& error)
{
    QMessageBox::warning(this, "Connection Error", error);
}

// ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================
void MainWindow::updateSendButtonState()
{
    // Кнопка отправки активна только при наличии текста и установленном соединении
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = ui->disconnectButton->isEnabled();
    ui->sendMessageButton->setEnabled(hasText && isConnected);
}

void MainWindow::updateImageButtonsState()
{
    // Кнопки обработки и очистки активны только при загруженном изображении
    bool hasImage = !ui->imagePathLabel->text().isEmpty() &&
        ui->imagePathLabel->text() != "No file selected";
    ui->processImageButton->setEnabled(hasImage);
    ui->clearImageButton->setEnabled(hasImage);
}

QString MainWindow::getNextAvailableFilename()
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

void MainWindow::updateNetNameEdit()
{
    QString nextFilename = getNextAvailableFilename();
    ui->netnamelineEdit->setText(nextFilename);
}