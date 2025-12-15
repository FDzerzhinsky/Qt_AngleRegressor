//  [file name]: MainWindow.cpp
#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "BusinessLogic.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QListWidgetItem>
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
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_businessLogic(new BusinessLogic)
    , m_businessThread(new QThread(this))
    , m_settings(nullptr)
    , m_socketConnected(false)
    , m_pairingTimer(new QTimer(this))
{
    ui->setupUi(this);

    // Устанавливаем стартовую вкладку (Сокет)
    ui->tabWidget->setCurrentIndex(0);

    // Инициализация настроек (при первом запуске создаст config.ini с секцией NeuralNetwork)
    initializeSettings();

    // Настройка потока бизнес-логики
    m_businessLogic->moveToThread(m_businessThread);

    // Настройка соединений сигналов и слотов
    setupConnections();

    // Запускаем поток и инициализацию бизнес-логики
    m_businessThread->start();
    QMetaObject::invokeMethod(m_businessLogic, "initialize", Qt::QueuedConnection);

    // Таймер для асинхронной обработки сопоставления
    m_pairingTimer->setSingleShot(true);
    connect(m_pairingTimer, &QTimer::timeout, this, &MainWindow::onProcessDataPairing);

    // Инициализация интерфейса
    updateSendButtonState();

    // Инициализация списка моделей на вкладке "Выбор модели"
    updateResultsList();
}

MainWindow::~MainWindow()
{
    // Корректное завершение потока
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
            out << "ref_image_path=reference.png\n";
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
            out << "\n";
            out << "[NeuralNetwork]\n";
            out << "model_path=can_angle_model.onnx\n";
            out << "input_width=256    # Ширина как у камеры\n";
            out << "input_height=536   # Высота как у камеры\n";
            out << "input_channels=1\n";
            out << "mean=0.485\n";
            out << "std=0.229\n";
            out << "num_classes=360\n";
            out << "use_gpu=false\n";
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
    // СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "СОКЕТ"
    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendMessageClicked);
    connect(ui->messageLineEdit, &QLineEdit::textChanged, this, &MainWindow::onMessageTextChanged);

    // СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "ВЫБОР МОДЕЛИ"
    connect(ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
        QWidget* w = ui->tabWidget->widget(index);
        if (w && w->objectName() == "resultsTab") {
            onResultsTabActivated();
        }
    });
    connect(ui->selectPatternButton, &QPushButton::clicked, this, &MainWindow::onSelectPatternClicked);
    connect(ui->resultsListWidget, &QListWidget::itemSelectionChanged, this, &MainWindow::onPatternSelectionChanged);

    // СОЕДИНЕНИЯ ДЛЯ ВКЛАДКИ "КОМПЬЮТЕРНОЕ ЗРЕНИЕ"
    connect(ui->startVisionButton, &QPushButton::clicked, this, &MainWindow::onStartVisionClicked);
    connect(ui->stopVisionButton, &QPushButton::clicked, this, &MainWindow::onStopVisionClicked);

    // НОВЫЕ СОЕДИНЕНИЯ ДЛЯ ЧЕКБОКСОВ
    connect(ui->saveSnapsCheckBox, &QCheckBox::toggled, this, &MainWindow::onSaveSnapshotsToggled);
    connect(ui->getFromSockCheckBox, &QCheckBox::toggled, this, &MainWindow::onGetFromSocketToggled);

    // СОЕДИНЕНИЯ С БИЗНЕС-ЛОГИКОЙ
    connect(m_businessLogic, &BusinessLogic::logMessage, this, &MainWindow::onLogMessage);
    connect(m_businessLogic, &BusinessLogic::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_businessLogic, &BusinessLogic::socketError, this, &MainWindow::onSocketError);
    connect(m_businessLogic, &BusinessLogic::socketDataReceived, this, &MainWindow::onSocketDataReceived);
    connect(m_businessLogic, &BusinessLogic::visionResultReceived, this, &MainWindow::onVisionResultReceived);
    connect(m_businessLogic, &BusinessLogic::visionResultReceivedForDisplay, this, &MainWindow::onVisionResultReceivedForDisplay);
    connect(m_businessLogic, &BusinessLogic::visionSystemError, this, &MainWindow::onVisionSystemError);
}

// Слот для асинхронной обработки сопоставления
void MainWindow::onProcessDataPairing()
{
    processDataPairing();
}

void MainWindow::processDataPairing()
{
    if (!ui->getFromSockCheckBox->isChecked() || !ui->saveSnapsCheckBox->isChecked()) {
        return;
    }

    QMutexLocker locker(&m_dataMutex);

    int snapshotCount = getSnapshotCount();
    int valuePairsCount = countValuePairs();

    qDebug() << "ProcessDataPairing - Snapshot count:" << snapshotCount << "Value pairs count:" << valuePairsCount << "Pending values:" << m_pendingSocketValues.size();

    if (snapshotCount > valuePairsCount && !m_pendingSocketValues.isEmpty()) {
        QStringList unpairedSnapshots = findUnpairedSnapshots();

        qDebug() << "Unpaired snapshots:" << unpairedSnapshots;

        while (!unpairedSnapshots.isEmpty() && !m_pendingSocketValues.isEmpty()) {
            QString snapshotName = unpairedSnapshots.takeFirst();
            QString socketValue = m_pendingSocketValues.takeFirst();

            saveValuePair(snapshotName, socketValue);

            qDebug() << "Paired snapshot:" << snapshotName << "with value:" << socketValue;
            onLogMessage(QString("Paired: %1 : %2").arg(snapshotName).arg(socketValue));
        }
    }
}

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

QStringList MainWindow::findUnpairedSnapshots()
{
    QDir snapsDir("snaps");
    QStringList unpairedSnapshots;

    if (!snapsDir.exists()) {
        qDebug() << "Snaps directory doesn't exist";
        return unpairedSnapshots;
    }

    QStringList filters;
    filters << "*.png";
    QFileInfoList fileList = snapsDir.entryInfoList(filters, QDir::Files, QDir::Time);

    if (fileList.isEmpty()) {
        qDebug() << "No snapshots found in snaps directory";
        return unpairedSnapshots;
    }

    QSet<QString> pairedSnapshots = readPairedSnapshotsFromValues();

    qDebug() << "Total snapshots:" << fileList.size() << "Paired snapshots:" << pairedSnapshots.size();

    for (const QFileInfo& fileInfo : fileList) {
        QString baseName = fileInfo.baseName();
        if (!pairedSnapshots.contains(baseName)) {
            unpairedSnapshots.append(baseName);
            qDebug() << "Found unpaired snapshot:" << baseName;
        }
    }

    unpairedSnapshots.sort();

    qDebug() << "Unpaired snapshots count:" << unpairedSnapshots.size();
    return unpairedSnapshots;
}

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

QString MainWindow::findLatestUnpairedSnapshot()
{
    QStringList unpaired = findUnpairedSnapshots();
    return unpaired.isEmpty() ? QString() : unpaired.first();
}

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

    if (checked) {
        QTimer::singleShot(100, this, &MainWindow::onProcessDataPairing);
    }
}

void MainWindow::onGetFromSocketToggled(bool checked)
{
    if (checked) {
        onLogMessage("Get from socket enabled - values will be saved to values.txt");
        QFile file("values.txt");
        if (!file.exists()) {
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                onLogMessage("Created values.txt file");
            }
        }
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

        QString trimmedData = data.trimmed();
        if (!trimmedData.isEmpty()) {
            m_pendingSocketValues.append(trimmedData);
        }

        onLogMessage(QString("Socket data received and queued: %1").arg(data));
        qDebug() << "Socket data received and queued:" << data;
        qDebug() << "Queue size:" << m_pendingSocketValues.size();

        if (!m_pairingTimer->isActive()) {
            m_pairingTimer->start(50);
        }
    }
}

void MainWindow::updateSendButtonState()
{
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = ui->disconnectButton->isEnabled();
    ui->sendMessageButton->setEnabled(hasText && isConnected);
}

// СЛОТЫ ДЛЯ ВКЛАДКИ "СОКЕТ"
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

void MainWindow::onMessageTextChanged(const QString& /*text*/)
{
    updateSendButtonState();
}

// ==================== ВКЛАДКА "ВЫБОР МОДЕЛИ" ====================
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
        // Сохраняем имя выбранной модели в General/selectedPattern (для совместимости)
        m_settings->setValue("General/selectedPattern", selectedFile);
        QString fullPath = "models/" + selectedFile;
        // Вписываем путь к модели в секцию [NeuralNetwork] model_path
        m_settings->setValue("NeuralNetwork/model_path", fullPath);
        m_settings->sync();
        onLogMessage("Выбрана модель: " + selectedFile);
        onLogMessage("NeuralNetwork.model_path обновлён: " + fullPath);
        QMessageBox::information(this, "Выбор модели", "Модель '" + selectedFile + "' выбрана и сохранена в настройках.");
    }
}

void MainWindow::onPatternSelectionChanged()
{
    ui->selectPatternButton->setEnabled(ui->resultsListWidget->currentItem() != nullptr);
}

void MainWindow::updateResultsList()
{
    ui->resultsListWidget->clear();
    QDir modelsDir("models");
    if (!modelsDir.exists()) {
        modelsDir.mkpath(".");
        return;
    }

    QStringList filters;
    filters << "*.onnx";
    QFileInfoList fileList = modelsDir.entryInfoList(filters, QDir::Files, QDir::Name);

    for (const QFileInfo& fileInfo : fileList) {
        QListWidgetItem* item = new QListWidgetItem(ui->resultsListWidget);
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
    if (m_settings) {
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
    if (!searchName.endsWith(".onnx", Qt::CaseInsensitive)) {
        searchName += ".onnx";
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

// ==================== ВАШИ СЛОТЫ ДЛЯ VISION/SOCKET/LOG остаются без изменений ====================
void MainWindow::onVisionResultReceived(const QString& /*snapshotName*/, int /*xPosition*/, double /*totalTime*/)
{
    if (ui->getFromSockCheckBox->isChecked() && ui->saveSnapsCheckBox->isChecked()) {
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

    if (connected) {
        onLogMessage("Socket connection established - get from socket feature available");
    }
    else {
        onLogMessage("Socket connection lost - get from socket feature disabled");
    }
}

void MainWindow::onSocketError(const QString& error)
{
    QMessageBox::warning(this, "Connection Error", error);
}

void MainWindow::onStartVisionClicked()
{
    qDebug() << "MainWindow::onStartVisionClicked() - Starting vision system";
    if (m_businessLogic) {
        m_businessLogic->startVisionSystem();
    }
    if (ui) {
        ui->startVisionButton->setEnabled(false);
        ui->stopVisionButton->setEnabled(true);
        ui->visionStatusLabel->setText("Status: Running");
        ui->visionResultsTextEdit->append("Vision system started...");
    }
}

void MainWindow::onStopVisionClicked()
{
    qDebug() << "MainWindow::onStopVisionClicked() - Stopping vision system";
    if (m_businessLogic) {
        m_businessLogic->stopVisionSystem();
    }
    if (ui) {
        ui->startVisionButton->setEnabled(true);
        ui->stopVisionButton->setEnabled(false);
        ui->visionStatusLabel->setText("Status: Stopped");
        ui->visionResultsTextEdit->append("Vision system stopped...");
    }
}