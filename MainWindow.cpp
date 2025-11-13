//  [file name]: MainWindow.cpp
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "BusinessLogic.h"
#include "CropDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QListWidgetItem>
#include <QPixmap>  
#include <QDirIterator>
#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_businessLogic(new BusinessLogic)
    , m_businessThread(new QThread(this))
    , m_cropDialog(new CropDialog(this))
    , m_settings(nullptr)
    , m_justSavedImage(false)
{
    ui->setupUi(this);

    // ==================== УСТАНОВКА СТАРТОВОЙ ВКЛАДКИ ====================
    ui->tabWidget->setCurrentIndex(0); // 0 = Сокет, 1 = Развёртка, 2 = Выбор рисунка

    // ==================== ИНИЦИАЛИЗАЦИЯ НАСТРОЕК ====================
    initializeSettings();

    // ==================== НАСТРОЙКА НАЧАЛЬНОГО СОСТОЯНИЯ ====================
    ui->messageLineEdit->setPlaceholderText("Enter message to send...");

    // ==================== НАСТРОЙКА ПОТОКА ДЛЯ БИЗНЕС-ЛОГИКИ ====================
    m_businessLogic->moveToThread(m_businessThread);

    // ==================== НАСТРОЙКА СОЕДИНЕНИЙ СИГНАЛОВ И СЛОТОВ ====================
    setupConnections();

    // ЗАПУСКАЕМ ПОТОК БИЗНЕС-ЛОГИКИ
    m_businessThread->start();

    // ВЫЗЫВАЕМ ИНИЦИАЛИЗАЦИЮ БИЗНЕС-ЛОГИКИ В ЕЕ ПОТОКЕ
    QMetaObject::invokeMethod(m_businessLogic, "initialize", Qt::QueuedConnection);

    // ==================== ИНИЦИАЛИЗАЦИЯ СОСТОЯНИЯ ИНТЕРФЕЙСА ====================
    updateSendButtonState();
    updateImageButtonsState();

    // Инициализация списка изображений на третьей вкладке
    updateResultsList();
    ui->SetDPILineEdit->setText("300");
    ui->SetDPILineEdit->setValidator(new QIntValidator(72, 1200, this));  // ОГРАНИЧЕНИЕ ДИАПАЗОНА
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

    // Сохраняем настройки перед выходом
    if (m_settings) {
        m_settings->sync();
        delete m_settings;
    }

    delete m_businessLogic;
    delete ui;
}

void MainWindow::initializeSettings()
{
    // =============================================================================
    // ИНИЦИАЛИЗАЦИЯ УНИФИЦИРОВАННОГО КОНФИГУРАЦИОННОГО ФАЙЛА
    // =============================================================================
    // ОПРЕДЕЛЯЕМ ПУТЬ К ФАЙЛУ НАСТРОЕК В ПАПКЕ С ИСПОЛНЯЕМЫМ ФАЙЛОМ
    // Это гарантирует, что конфиг всегда будет рядом с exe-файлом
    QString configPath = QApplication::applicationDirPath() + "/config.ini";
    qDebug() << "Config file path:" << configPath;

    // Явно создаем файл настроек с полной структурой, если его нет
    if (!QFile::exists(configPath)) {
        QFile configFile(configPath);
        if (configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&configFile);
            out << "[General]\n";
            out << "selectedPattern=\n";
            out << "\n";
            out << "[Camera]\n";
            out << "camera_ip=192.168.101.10\n";
            out << "host_ip=192.168.101.201\n";
            out << "max_captures=0\n";
            out << "\n";
            out << "[ImageProcessing]\n";
            // Автоматически выбираем первое изображение из netsurfaces если папка существует
            QDir netsurfacesDir("netsurfaces");
            QString refImagePath = "reference.png";
            if (netsurfacesDir.exists() && !netsurfacesDir.entryList(QStringList() << "*.png" << "*.jpg" << "*.jpeg", QDir::Files).isEmpty()) {
                QString firstImage = netsurfacesDir.entryList(QStringList() << "*.png" << "*.jpg" << "*.jpeg", QDir::Files).first();
                refImagePath = "netsurfaces/" + firstImage;
            }
            out << "ref_image_path=" << refImagePath << "\n";
            out << "ref_image_scale=0.5\n";
            out << "orb_max_features=500\n";
            out << "orb_scale_factor=1.2\n";
            out << "orb_n_levels=4\n";
            out << "orb_edge_threshold=20\n";
            out << "orb_first_level=0\n";
            out << "orb_wta_k=2\n";
            out << "orb_score_type=1\n";
            out << "orb_patch_size=31\n";
            out << "orb_fast_threshold=20\n";
            out << "flann_search_params=10\n";
            out << "good_match_ratio=0.65\n";
            out << "min_good_matches=5\n";
            out << "ransac_threshold=3.0\n";
            out << "save_snapshots=false\n";
            configFile.close();
            qDebug() << "Config file created at:" << configPath;
        }
        else {
            qWarning() << "Failed to create config file at:" << configPath;
        }
    }
    else {
        // Файл уже существует - проверяем и дополняем его структуру
        QSettings tempSettings(configPath, QSettings::IniFormat);

        // Если нет ключа selectedPattern - добавляем его
        if (!tempSettings.contains("General/selectedPattern")) {
            tempSettings.setValue("General/selectedPattern", "");
        }

        // Проверяем и добавляем отсутствующие ключи в секцию Camera
        if (!tempSettings.contains("Camera/camera_ip")) {
            tempSettings.setValue("Camera/camera_ip", "192.168.101.10");
        }
        if (!tempSettings.contains("Camera/host_ip")) {
            tempSettings.setValue("Camera/host_ip", "192.168.101.201");
        }
        if (!tempSettings.contains("Camera/max_captures")) {
            tempSettings.setValue("Camera/max_captures", 0);
        }

        // Проверяем и добавляем отсутствующие ключи в секцию ImageProcessing
        if (!tempSettings.contains("ImageProcessing/ref_image_path")) {
            // Устанавливаем путь к первому доступному изображению в netsurfaces
            QDir netsurfacesDir("netsurfaces");
            QString refImagePath = "reference.png";
            if (netsurfacesDir.exists() && !netsurfacesDir.entryList(QStringList() << "*.png" << "*.jpg" << "*.jpeg", QDir::Files).isEmpty()) {
                QString firstImage = netsurfacesDir.entryList(QStringList() << "*.png" << "*.jpg" << "*.jpeg", QDir::Files).first();
                refImagePath = "netsurfaces/" + firstImage;
            }
            tempSettings.setValue("ImageProcessing/ref_image_path", refImagePath);
        }
        if (!tempSettings.contains("ImageProcessing/ref_image_scale")) {
            tempSettings.setValue("ImageProcessing/ref_image_scale", 0.5);
        }
        if (!tempSettings.contains("ImageProcessing/orb_max_features")) {
            tempSettings.setValue("ImageProcessing/orb_max_features", 500);
        }
        if (!tempSettings.contains("ImageProcessing/orb_scale_factor")) {
            tempSettings.setValue("ImageProcessing/orb_scale_factor", 1.2);
        }
        if (!tempSettings.contains("ImageProcessing/orb_n_levels")) {
            tempSettings.setValue("ImageProcessing/orb_n_levels", 4);
        }
        if (!tempSettings.contains("ImageProcessing/orb_edge_threshold")) {
            tempSettings.setValue("ImageProcessing/orb_edge_threshold", 20);
        }
        if (!tempSettings.contains("ImageProcessing/orb_first_level")) {
            tempSettings.setValue("ImageProcessing/orb_first_level", 0);
        }
        if (!tempSettings.contains("ImageProcessing/orb_wta_k")) {
            tempSettings.setValue("ImageProcessing/orb_wta_k", 2);
        }
        if (!tempSettings.contains("ImageProcessing/orb_score_type")) {
            tempSettings.setValue("ImageProcessing/orb_score_type", 1);
        }
        if (!tempSettings.contains("ImageProcessing/orb_patch_size")) {
            tempSettings.setValue("ImageProcessing/orb_patch_size", 31);
        }
        if (!tempSettings.contains("ImageProcessing/orb_fast_threshold")) {
            tempSettings.setValue("ImageProcessing/orb_fast_threshold", 20);
        }
        if (!tempSettings.contains("ImageProcessing/flann_search_params")) {
            tempSettings.setValue("ImageProcessing/flann_search_params", 10);
        }
        if (!tempSettings.contains("ImageProcessing/good_match_ratio")) {
            tempSettings.setValue("ImageProcessing/good_match_ratio", 0.65);
        }
        if (!tempSettings.contains("ImageProcessing/min_good_matches")) {
            tempSettings.setValue("ImageProcessing/min_good_matches", 5);
        }
        if (!tempSettings.contains("ImageProcessing/ransac_threshold")) {
            tempSettings.setValue("ImageProcessing/ransac_threshold", 3.0);
        }
        if (!tempSettings.contains("ImageProcessing/save_snapshots")) {
            tempSettings.setValue("ImageProcessing/save_snapshots", false);
        }

        tempSettings.sync();
        qDebug() << "Existing config file updated with missing keys";
    }

    // Инициализируем QSettings с явным указанием пути к файлу
    m_settings = new QSettings(configPath, QSettings::IniFormat, this);
    qDebug() << "Using config file:" << m_settings->fileName();

    // Логируем текущие настройки для отладки
    qDebug() << "Current settings:";
    QStringList allKeys = m_settings->allKeys();
    for (const QString& key : allKeys) {
        qDebug() << "  " << key << "=" << m_settings->value(key).toString();
    }
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

    // ==================== СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "ВЫБОР РИСУНКА" ====================
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 2) { // Индекс вкладки "Выбор рисунка"
            onResultsTabActivated();
        }
        });
    connect(ui->selectPatternButton, &QPushButton::clicked, this, &MainWindow::onSelectPatternClicked);
    connect(ui->resultsListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onPatternSelectionChanged);
    connect(ui->SetDPILineEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        bool ok;
        int dpi = text.toInt(&ok);
        if (ok) {
            m_businessLogic->setPdfRenderingDpi(dpi);
        }
        });

    // ==================== СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "КОМПЬЮТЕРНОЕ ЗРЕНИЕ" ====================
    // ВАЖНО: Подключаем кнопки компьютерного зрения напрямую
    connect(ui->startVisionButton, &QPushButton::clicked, this, &MainWindow::onStartVisionClicked);
    connect(ui->stopVisionButton, &QPushButton::clicked, this, &MainWindow::onStopVisionClicked);

    // =============================================================================
    // НОВОЕ СОЕДИНЕНИЕ ДЛЯ ЧЕКБОКСА СОХРАНЕНИЯ СНЭПШОТОВ
    // =============================================================================
    connect(ui->saveSnapsCheckBox, &QCheckBox::toggled, m_businessLogic, &BusinessLogic::setSaveSnapshots);

    // ==================== СОЕДИНЕНИЯ С БИЗНЕС-ЛОГИКОЙ ====================
    // Сигналы от бизнес-логики к GUI (межпоточные соединения)
    connect(m_businessLogic, &BusinessLogic::logMessage, this, &MainWindow::onLogMessage);
    connect(m_businessLogic, &BusinessLogic::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_businessLogic, &BusinessLogic::imageLoaded, this, &MainWindow::onImageLoaded);
    connect(m_businessLogic, &BusinessLogic::imageProcessed, this, &MainWindow::onImageProcessed);
    connect(m_businessLogic, &BusinessLogic::imageCleared, this, &MainWindow::onImageCleared);
    connect(m_businessLogic, &BusinessLogic::socketError, this, &MainWindow::onSocketError);

    // ==================== СОЕДИНЕНИЯ С БИЗНЕС-ЛОГИКОЙ ДЛЯ КОМПЬЮТЕРНОГО ЗРЕНИЯ ====================
    connect(m_businessLogic, &BusinessLogic::visionResultReceived,
        this, &MainWindow::onVisionResultReceived);

    // =============================================================================
    // НОВОЕ СОЕДИНЕНИЕ ДЛЯ ОТОБРАЖЕНИЯ РЕЗУЛЬТАТОВ ОБРАБОТКИ В GUI
    // =============================================================================
    connect(m_businessLogic, &BusinessLogic::visionResultReceivedForDisplay,
        this, &MainWindow::onVisionResultReceivedForDisplay);

    connect(m_businessLogic, &BusinessLogic::visionSystemError,
        this, &MainWindow::onVisionSystemError);
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

        // Сохраняем имя файла для выделения на третьей вкладке и устанавливаем флаг
        m_lastSavedImage = filename;
        m_justSavedImage = true;

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

// ==================== РЕАЛИЗАЦИЯ СЛОТОВ ДЛЯ ВКЛАДКИ "ВЫБОР РИСУНКА" ====================
void MainWindow::onResultsTabActivated()
{
    updateResultsList();
    selectDefaultPattern();
}

void MainWindow::onSelectPatternClicked()
{
    QListWidgetItem* currentItem = ui->resultsListWidget->currentItem();
    if (currentItem && m_settings) {
        QString selectedFile = currentItem->data(Qt::UserRole).toString();

        // Сохраняем выбранный паттерн в секции General
        m_settings->setValue("General/selectedPattern", selectedFile);

        // Обновляем путь к референсному изображению в секции ImageProcessing
        QString fullPath = "netsurfaces/" + selectedFile;
        m_settings->setValue("ImageProcessing/ref_image_path", fullPath);

        m_settings->sync(); // Явно сохраняем изменения

        onLogMessage("Выбран рисунок: " + selectedFile);
        onLogMessage("Референсное изображение обновлено: " + fullPath);
        QMessageBox::information(this, "Выбор рисунка", "Рисунок '" + selectedFile + "' выбран и сохранен в настройках.");
    }
}

void MainWindow::onPatternSelectionChanged()
{
    ui->selectPatternButton->setEnabled(ui->resultsListWidget->currentItem() != nullptr);
}

// ==================== РЕАЛИЗАЦИЯ СЛОТОВ ДЛЯ КОМПЬЮТЕРНОГО ЗРЕНИЯ ====================
void MainWindow::onStartVisionClicked()
{
    qDebug() << "MainWindow::onStartVisionClicked() - Starting vision system";
    m_businessLogic->startVisionSystem();
    ui->startVisionButton->setEnabled(false);
    ui->stopVisionButton->setEnabled(true);
    ui->visionStatusLabel->setText("Status: Running");
    ui->visionResultsTextEdit->append("Vision system started...");
}

void MainWindow::onStopVisionClicked()
{
    qDebug() << "MainWindow::onStopVisionClicked() - Stopping vision system";
    m_businessLogic->stopVisionSystem();
    ui->startVisionButton->setEnabled(true);
    ui->stopVisionButton->setEnabled(false);
    ui->visionStatusLabel->setText("Status: Stopped");
    ui->visionResultsTextEdit->append("Vision system stopped...");
}

void MainWindow::onVisionResultReceived(const QString& snapshotName, int xPosition, double totalTime)
{
    // Этот слот теперь не используется для отображения в GUI
    // Вместо него используется visionResultReceivedForDisplay
    Q_UNUSED(snapshotName)
        Q_UNUSED(xPosition)
        Q_UNUSED(totalTime)
}

// =============================================================================
// НОВЫЙ СЛОТ ДЛЯ ОТОБРАЖЕНИЯ РЕЗУЛЬТАТОВ ОБРАБОТКИ В GUI
// =============================================================================
void MainWindow::onVisionResultReceivedForDisplay(const QString& displayMessage)
{
    ui->visionResultsTextEdit->append(displayMessage);
}

void MainWindow::onVisionSystemError(const QString& error)
{
    ui->visionResultsTextEdit->append(QString("[ERROR] %1").arg(error));
    QMessageBox::warning(this, "Vision System Error", error);
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

    // Обновляем список на третьей вкладке
    updateResultsList();

    // Выделяем только что сохраненный файл
    if (m_justSavedImage && !m_lastSavedImage.isEmpty()) {
        selectFileInList(m_lastSavedImage);
        m_justSavedImage = false; // Сбрасываем флаг после использования
    }

    // Переключаемся на вкладку "Выбор рисунка"
    ui->tabWidget->setCurrentIndex(2);
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
    // Кнопки обработки и очистки активны только при загруженным изображении
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

void MainWindow::updateResultsList()
{
    ui->resultsListWidget->clear();

    QDir netsurfacesDir("netsurfaces");
    if (!netsurfacesDir.exists()) {
        netsurfacesDir.mkpath(".");
        return;
    }

    // Поддерживаемые форматы изображений
    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tiff";

    QFileInfoList fileList = netsurfacesDir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo& fileInfo : fileList) {
        QListWidgetItem* item = new QListWidgetItem(ui->resultsListWidget);

        // Загружаем уменьшенное изображение для превью
        QPixmap pixmap(fileInfo.absoluteFilePath());
        if (!pixmap.isNull()) {
            // Масштабируем до размера иконки
            QPixmap scaledPixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            item->setIcon(QIcon(scaledPixmap));
        }

        item->setText(fileInfo.fileName());
        item->setData(Qt::UserRole, fileInfo.fileName());

        // Показываем полное имя файла как подсказку
        item->setToolTip(fileInfo.fileName());
    }
}

void MainWindow::selectDefaultPattern()
{
    if (ui->resultsListWidget->count() == 0) {
        ui->selectPatternButton->setEnabled(false);
        return;
    }

    QString patternToSelect;

    // Если только что сохранили изображение - выделяем его
    if (m_justSavedImage && !m_lastSavedImage.isEmpty()) {
        patternToSelect = m_lastSavedImage;
        m_justSavedImage = false; // Сбрасываем флаг после использования
    }
    else if (m_settings) {
        // Иначе читаем из конфига
        patternToSelect = m_settings->value("General/selectedPattern").toString();
    }

    // Ищем файл в списке
    if (!patternToSelect.isEmpty()) {
        selectFileInList(patternToSelect);
        return;
    }

    // Если не нашли или конфиг пуст - выделяем первый элемент
    if (ui->resultsListWidget->count() > 0) {
        ui->resultsListWidget->setCurrentRow(0);
    }
}

void MainWindow::selectFileInList(const QString& fileName)
{
    // Убедимся, что имя файла имеет расширение .png
    QString searchName = fileName;
    if (!searchName.endsWith(".png", Qt::CaseInsensitive)) {
        searchName += ".png";
    }

    // Ищем файл в списке
    QList<QListWidgetItem*> items = ui->resultsListWidget->findItems(searchName, Qt::MatchExactly);
    if (!items.isEmpty()) {
        items.first()->setSelected(true);
        ui->resultsListWidget->setCurrentItem(items.first());
        ui->resultsListWidget->scrollToItem(items.first());
    }
    else {
        // Если не нашли точное совпадение, выделяем первый элемент
        if (ui->resultsListWidget->count() > 0) {
            ui->resultsListWidget->setCurrentRow(0);
        }
    }
}