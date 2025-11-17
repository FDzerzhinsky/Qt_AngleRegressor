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
#include <QMutexLocker>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_businessLogic(new BusinessLogic)
    , m_businessThread(new QThread(this))
    , m_cropDialog(new CropDialog(this))
    , m_settings(nullptr)
    , m_justSavedImage(false)
    , m_socketConnected(false)
    , m_pairingTimer(new QTimer(this))
{
    ui->setupUi(this);

    // ==================== УСТАНОВКА СТАРТОВОЙ ВКЛАДКИ ====================
    ui->tabWidget->setCurrentIndex(0);

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

    // ==================== НАСТРОЙКА ТАЙМЕРА ДЛЯ АСИНХРОННОЙ ОБРАБОТКИ ====================
    m_pairingTimer->setSingleShot(true);
    connect(m_pairingTimer, &QTimer::timeout, this, &MainWindow::onProcessDataPairing);

    // ==================== ИНИЦИАЛИЗАЦИЯ СОСТОЯНИЯ ИНТЕРФЕЙСА ====================
    updateSendButtonState();
    updateImageButtonsState();
    updateGetFromSocketState();

    // Инициализация списка изображений на третьей вкладке
    updateResultsList();
    ui->SetDPILineEdit->setText("300");
    ui->SetDPILineEdit->setValidator(new QIntValidator(72, 1200, this));
}

MainWindow::~MainWindow()
{
    // ==================== КОРРЕКТНОЕ ЗАВЕРШЕНИЕ ПОТОКА ====================
    m_businessThread->quit();
    m_businessThread->wait(1000);

    if (m_businessThread->isRunning()) {
        m_businessThread->terminate();
        m_businessThread->wait();
    }

    if (m_settings) {
        m_settings->sync();
        delete m_settings;
    }

    delete m_businessLogic;
    delete ui;
}

void MainWindow::initializeSettings()
{
    QString configPath = QApplication::applicationDirPath() + "/config.ini";
    qDebug() << "Config file path:" << configPath;

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

    m_settings = new QSettings(configPath, QSettings::IniFormat, this);
    qDebug() << "Using config file:" << m_settings->fileName();
}

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
        if (index == 2) {
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
    connect(ui->startVisionButton, &QPushButton::clicked, this, &MainWindow::onStartVisionClicked);
    connect(ui->stopVisionButton, &QPushButton::clicked, this, &MainWindow::onStopVisionClicked);

    // =============================================================================
    // НОВЫЕ СОЕДИНЕНИЯ ДЛЯ ЧЕКБОКСОВ
    // =============================================================================
    connect(ui->saveSnapsCheckBox, &QCheckBox::toggled, this, &MainWindow::onSaveSnapshotsToggled);
    connect(ui->getFromSockCheckBox, &QCheckBox::toggled, this, &MainWindow::onGetFromSocketToggled);

    // ==================== СОЕДИНЕНИЯ С БИЗНЕС-ЛОГИКОЙ ====================
    connect(m_businessLogic, &BusinessLogic::logMessage, this, &MainWindow::onLogMessage);
    connect(m_businessLogic, &BusinessLogic::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_businessLogic, &BusinessLogic::imageLoaded, this, &MainWindow::onImageLoaded);
    connect(m_businessLogic, &BusinessLogic::imageProcessed, this, &MainWindow::onImageProcessed);
    connect(m_businessLogic, &BusinessLogic::imageCleared, this, &MainWindow::onImageCleared);
    connect(m_businessLogic, &BusinessLogic::socketError, this, &MainWindow::onSocketError);
    connect(m_businessLogic, &BusinessLogic::socketDataReceived, this, &MainWindow::onSocketDataReceived);
    connect(m_businessLogic, &BusinessLogic::visionResultReceived, this, &MainWindow::onVisionResultReceived);
    connect(m_businessLogic, &BusinessLogic::visionResultReceivedForDisplay, this, &MainWindow::onVisionResultReceivedForDisplay);
    connect(m_businessLogic, &BusinessLogic::visionSystemError, this, &MainWindow::onVisionSystemError);
}

// =============================================================================
// СЛОТ ДЛЯ АСИНХРОННОЙ ОБРАБОТКИ СОПОСТАВЛЕНИЯ (РЕШЕНИЕ ПРОБЛЕМЫ БЛОКИРОВКИ GUI)
// =============================================================================
void MainWindow::onProcessDataPairing()
{
    processDataPairing();
}

// =============================================================================
// ОСНОВНОЙ МЕТОД ДЛЯ СОПОСТАВЛЕНИЯ СНЭПШОТОВ И ЗНАЧЕНИЙ ИЗ СОКЕТА
// =============================================================================
void MainWindow::processDataPairing()
{
    if (!ui->getFromSockCheckBox->isChecked() || !ui->saveSnapsCheckBox->isChecked()) {
        return;
    }

    QMutexLocker locker(&m_dataMutex);

    // Получаем количество снэпшотов и записей в values.txt
    int snapshotCount = getSnapshotCount();
    int valuePairsCount = countValuePairs();

    qDebug() << "ProcessDataPairing - Snapshot count:" << snapshotCount << "Value pairs count:" << valuePairsCount << "Pending values:" << m_pendingSocketValues.size();

    // Если снэпшотов больше чем записей И есть ожидающие значения - сопоставляем
    if (snapshotCount > valuePairsCount && !m_pendingSocketValues.isEmpty()) {
        // Находим все несопоставленные снэпшоты
        QStringList unpairedSnapshots = findUnpairedSnapshots();

        qDebug() << "Unpaired snapshots:" << unpairedSnapshots;

        // Сопоставляем по порядку - первый несопоставленный снэпшот с первым значением из очереди
        while (!unpairedSnapshots.isEmpty() && !m_pendingSocketValues.isEmpty()) {
            QString snapshotName = unpairedSnapshots.takeFirst();
            QString socketValue = m_pendingSocketValues.takeFirst();

            // Сохраняем пару
            saveValuePair(snapshotName, socketValue);

            qDebug() << "Paired snapshot:" << snapshotName << "with value:" << socketValue;
            onLogMessage(QString("Paired: %1 : %2").arg(snapshotName).arg(socketValue));
        }
    }
}

// =============================================================================
// МЕТОД ДЛЯ СОХРАНЕНИЯ ПАРЫ В ФАЙЛ values.txt
// =============================================================================
void MainWindow::saveValuePair(const QString& snapshotName, const QString& socketValue)
{
    QFile file("values.txt");
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        QString line = QString("%1 : %2\n").arg(snapshotName).arg(socketValue);
        out << line;
        file.close();

        onLogMessage(QString("Saved to values.txt: %1 : %2").arg(snapshotName).arg(socketValue));
        qDebug() << "Saved value pair:" << line;
    }
    else {
        onLogMessage("ERROR: Failed to open values.txt for writing");
        qWarning() << "Failed to open values.txt for writing";
    }
}

// =============================================================================
// ПОИСК ВСЕХ НЕСОПОСТАВЛЕННЫХ СНЭПШОТОВ
// =============================================================================
QStringList MainWindow::findUnpairedSnapshots()
{
    QDir snapsDir("snaps");
    QStringList unpairedSnapshots;

    if (!snapsDir.exists()) {
        qDebug() << "Snaps directory doesn't exist";
        return unpairedSnapshots;
    }

    // Получаем все PNG файлы, отсортированные по времени создания (старые первыми)
    QStringList filters;
    filters << "*.png";
    QFileInfoList fileList = snapsDir.entryInfoList(filters, QDir::Files, QDir::Time);

    if (fileList.isEmpty()) {
        qDebug() << "No snapshots found in snaps directory";
        return unpairedSnapshots;
    }

    // Читаем уже сопоставленные снэпшоты из values.txt
    QSet<QString> pairedSnapshots = readPairedSnapshotsFromValues();

    qDebug() << "Total snapshots:" << fileList.size() << "Paired snapshots:" << pairedSnapshots.size();

    // Находим все несопоставленные снэпшоты
    for (const QFileInfo& fileInfo : fileList) {
        QString baseName = fileInfo.baseName();
        if (!pairedSnapshots.contains(baseName)) {
            unpairedSnapshots.append(baseName);
            qDebug() << "Found unpaired snapshot:" << baseName;
        }
    }

    // Сортируем по времени (старые первыми) чтобы сопоставлять в правильном порядке
    unpairedSnapshots.sort();

    qDebug() << "Unpaired snapshots count:" << unpairedSnapshots.size();
    return unpairedSnapshots;
}

// =============================================================================
// ЧТЕНИЕ УЖЕ СОПОСТАВЛЕННЫХ СНЭПШОТОВ ИЗ ФАЙЛА values.txt
// =============================================================================
QSet<QString> MainWindow::readPairedSnapshotsFromValues()
{
    QSet<QString> pairedSnapshots;

    QFile file("values.txt");
    if (!file.exists()) {
        return pairedSnapshots;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.isEmpty()) continue;

            // Разбираем строку формата "snap_2025-11-14_17-01-29 : SAMPLE_TEXT"
            QStringList parts = line.split(" : ");
            if (parts.size() >= 1) {
                QString snapshotName = parts[0].trimmed();
                pairedSnapshots.insert(snapshotName);
                qDebug() << "Found paired snapshot in values.txt:" << snapshotName;
            }
        }
        file.close();
    }

    return pairedSnapshots;
}

// =============================================================================
// ПОИСК ПОСЛЕДНЕГО НЕСОПОСТАВЛЕННОГО СНЭПШОТА
// =============================================================================
QString MainWindow::findLatestUnpairedSnapshot()
{
    QStringList unpaired = findUnpairedSnapshots();
    // Возвращаем самый старый несопоставленный снэпшот (первый в отсортированном списке)
    return unpaired.isEmpty() ? QString() : unpaired.first();
}

// =============================================================================
// ПОДСЧЕТ КОЛИЧЕСТВА СНЭПШОТОВ В ПАПКЕ SNAPS
// =============================================================================
int MainWindow::getSnapshotCount()
{
    QDir snapsDir("snaps");
    if (!snapsDir.exists()) {
        return 0;
    }

    QStringList filters;
    filters << "*.png";
    return snapsDir.entryList(filters, QDir::Files).count();
}

// =============================================================================
// ПОДСЧЕТ КОЛИЧЕСТВА ЗАПИСЕЙ В ФАЙЛЕ values.txt
// =============================================================================
int MainWindow::countValuePairs()
{
    QFile file("values.txt");
    if (!file.exists()) {
        return 0;
    }

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        int count = 0;
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                count++;
            }
        }
        file.close();
        return count;
    }

    return 0;
}

void MainWindow::onSaveSnapshotsToggled(bool checked)
{
    m_businessLogic->setSaveSnapshots(checked);
    updateGetFromSocketState();

    // Если включили сохранение снэпшотов, запускаем проверку сопоставления
    if (checked) {
        QTimer::singleShot(100, this, &MainWindow::onProcessDataPairing);
    }
}

void MainWindow::onGetFromSocketToggled(bool checked)
{
    if (checked) {
        onLogMessage("Get from socket enabled - values will be saved to values.txt");
        // Создаем файл values.txt если его нет
        QFile file("values.txt");
        if (!file.exists()) {
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                onLogMessage("Created values.txt file");
            }
        }

        // Запускаем проверку сопоставления
        QTimer::singleShot(100, this, &MainWindow::onProcessDataPairing);
    }
    else {
        onLogMessage("Get from socket disabled");
    }
}

void MainWindow::onSocketDataReceived(const QString& data)
{
    if (ui->getFromSockCheckBox->isChecked() && ui->saveSnapsCheckBox->isChecked()) {
        QMutexLocker locker(&m_dataMutex);

        // Добавляем значение в список ожидания
        QString trimmedData = data.trimmed();
        if (!trimmedData.isEmpty()) {
            m_pendingSocketValues.append(trimmedData);
        }

        onLogMessage(QString("Socket data received and queued: %1").arg(data));
        qDebug() << "Socket data received and queued:" << data;
        qDebug() << "Queue size:" << m_pendingSocketValues.size();

        // ЗАПУСКАЕМ АСИНХРОННУЮ ОБРАБОТКУ ЧЕРЕЗ ТАЙМЕР (РЕШЕНИЕ ПРОБЛЕМЫ БЛОКИРОВКИ GUI)
        if (!m_pairingTimer->isActive()) {
            m_pairingTimer->start(50); // Запускаем через 50 мс
        }
    }
}

void MainWindow::updateGetFromSocketState()
{
    bool isEnabled = ui->saveSnapsCheckBox->isChecked() && m_socketConnected;
    ui->getFromSockCheckBox->setEnabled(isEnabled);

    if (!isEnabled) {
        ui->getFromSockCheckBox->setChecked(false);
    }
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
        m_lastSavedImage = filename;
        m_justSavedImage = true;
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
        m_settings->setValue("General/selectedPattern", selectedFile);
        QString fullPath = "netsurfaces/" + selectedFile;
        m_settings->setValue("ImageProcessing/ref_image_path", fullPath);
        m_settings->sync();
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
    Q_UNUSED(snapshotName)
        Q_UNUSED(xPosition)
        Q_UNUSED(totalTime)

        // При получении результата обработки кадра пытаемся сопоставить данные
        if (ui->getFromSockCheckBox->isChecked() && ui->saveSnapsCheckBox->isChecked()) {
            // ЗАПУСКАЕМ АСИНХРОННУЮ ОБРАБОТКУ ЧЕРЕЗ ТАЙМЕР
            if (!m_pairingTimer->isActive()) {
                m_pairingTimer->start(50);
            }
        }
}

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
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    m_socketConnected = connected;
    updateSendButtonState();
    updateGetFromSocketState();

    if (connected) {
        onLogMessage("Socket connection established - get from socket feature available");
    }
    else {
        onLogMessage("Socket connection lost - get from socket feature disabled");
    }
}

void MainWindow::onImageLoaded(const QPixmap& originalImage, const QString& fileName)
{
    m_currentImage = originalImage;
    m_cropDialog->setImage(originalImage);

    if (m_cropDialog->exec() == QDialog::Accepted) {
        m_croppedImage = m_cropDialog->getCroppedImage();
        QSize labelSize = ui->imagePreviewLabel->size();
        QPixmap scaledImage = m_croppedImage.scaled(
            labelSize.width() - 10,
            labelSize.height() - 10,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );

        ui->imagePreviewLabel->setPixmap(scaledImage);

        if (m_cropDialog->getCroppedImage().size() != originalImage.size()) {
            ui->imagePathLabel->setText(fileName + " (cropped)");
        }
        else {
            ui->imagePathLabel->setText(fileName);
        }

        updateNetNameEdit();
        updateImageButtonsState();
        ui->tabWidget->setCurrentIndex(1);
    }
}

void MainWindow::onImageProcessed()
{
    ui->processImageButton->setEnabled(true);
    updateResultsList();

    if (m_justSavedImage && !m_lastSavedImage.isEmpty()) {
        selectFileInList(m_lastSavedImage);
        m_justSavedImage = false;
    }

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
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = ui->disconnectButton->isEnabled();
    ui->sendMessageButton->setEnabled(hasText && isConnected);
}

void MainWindow::updateImageButtonsState()
{
    bool hasImage = !ui->imagePathLabel->text().isEmpty() &&
        ui->imagePathLabel->text() != "No file selected";
    ui->processImageButton->setEnabled(hasImage);
    ui->clearImageButton->setEnabled(hasImage);
}

QString MainWindow::getNextAvailableFilename()
{
    QDir netsurfacesDir("netsurfaces");
    if (!netsurfacesDir.exists()) {
        netsurfacesDir.mkpath(".");
    }

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

    QStringList filters;
    filters << "*.png" << "*.jpg" << "*.jpeg" << "*.bmp" << "*.tiff";
    QFileInfoList fileList = netsurfacesDir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo& fileInfo : fileList) {
        QListWidgetItem* item = new QListWidgetItem(ui->resultsListWidget);
        QPixmap pixmap(fileInfo.absoluteFilePath());
        if (!pixmap.isNull()) {
            QPixmap scaledPixmap = pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            item->setIcon(QIcon(scaledPixmap));
        }
        item->setText(fileInfo.fileName());
        item->setData(Qt::UserRole, fileInfo.fileName());
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
    if (m_justSavedImage && !m_lastSavedImage.isEmpty()) {
        patternToSelect = m_lastSavedImage;
        m_justSavedImage = false;
    }
    else if (m_settings) {
        patternToSelect = m_settings->value("General/selectedPattern").toString();
    }

    if (!patternToSelect.isEmpty()) {
        selectFileInList(patternToSelect);
        return;
    }

    if (ui->resultsListWidget->count() > 0) {
        ui->resultsListWidget->setCurrentRow(0);
    }
}

void MainWindow::selectFileInList(const QString& fileName)
{
    QString searchName = fileName;
    if (!searchName.endsWith(".png", Qt::CaseInsensitive)) {
        searchName += ".png";
    }

    QList<QListWidgetItem*> items = ui->resultsListWidget->findItems(searchName, Qt::MatchExactly);
    if (!items.isEmpty()) {
        items.first()->setSelected(true);
        ui->resultsListWidget->setCurrentItem(items.first());
        ui->resultsListWidget->scrollToItem(items.first());
    }
    else {
        if (ui->resultsListWidget->count() > 0) {
            ui->resultsListWidget->setCurrentRow(0);
        }
    }
}