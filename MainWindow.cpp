#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "BusinessLogic.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_businessLogic(new BusinessLogic)
    , m_businessThread(new QThread(this))
{
    ui->setupUi(this);

    // ==================== ÍÀÑÒÐÎÉÊÀ ÍÀ×ÀËÜÍÎÃÎ ÑÎÑÒÎßÍÈß ====================

    ui->messageLineEdit->setPlaceholderText("Enter message to send...");

    // ==================== ÍÀÑÒÐÎÉÊÀ ÏÎÒÎÊÀ ÄËß ÁÈÇÍÅÑ-ËÎÃÈÊÈ ====================

    // Ïåðåìåùàåì áèçíåñ-ëîãèêó â îòäåëüíûé ïîòîê
    m_businessLogic->moveToThread(m_businessThread);

    // Çàïóñêàåì ïîòîê
    m_businessThread->start();

    // ==================== ÍÀÑÒÐÎÉÊÀ ÑÎÅÄÈÍÅÍÈÉ ====================

    setupConnections();

    // ==================== ÈÍÈÖÈÀËÈÇÀÖÈß ÑÎÑÒÎßÍÈß ====================

    updateSendButtonState();
    updateImageButtonsState();
}

MainWindow::~MainWindow()
{
    // Êîððåêòíîå çàâåðøåíèå ïîòîêà
    m_businessThread->quit();
    m_businessThread->wait(1000); // Æäåì äî 1 ñåêóíäû äëÿ çàâåðøåíèÿ

    if (m_businessThread->isRunning()) {
        m_businessThread->terminate();
        m_businessThread->wait();
    }

    delete m_businessLogic;
    delete ui;
}

// ==================== ÍÀÑÒÐÎÉÊÀ ÑÎÅÄÈÍÅÍÈÉ ÌÅÆÄÓ GUI È ÁÈÇÍÅÑ-ËÎÃÈÊÎÉ ====================

void MainWindow::setupConnections()
{
    // ==================== ÑÎÅÄÈÍÅÍÈß ÄËß ÂÊËÀÄÊÈ "ÑÎÊÅÒ" ====================

    connect(ui->connectButton, &QPushButton::clicked, this, &MainWindow::onConnectClicked);
    connect(ui->disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectClicked);
    connect(ui->clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLogClicked);
    connect(ui->sendMessageButton, &QPushButton::clicked, this, &MainWindow::onSendMessageClicked);
    connect(ui->messageLineEdit, &QLineEdit::textChanged, this, &MainWindow::onMessageTextChanged);

    // ==================== ÑÎÅÄÈÍÅÍÈß ÄËß ÂÊËÀÄÊÈ "ÐÀÇÂ¨ÐÒÊÀ" ====================

    connect(ui->loadImageButton, &QPushButton::clicked, this, &MainWindow::onLoadImageClicked);
    connect(ui->processImageButton, &QPushButton::clicked, this, &MainWindow::onProcessImageClicked);
    connect(ui->clearImageButton, &QPushButton::clicked, this, &MainWindow::onClearImageClicked);

    // ==================== ÑÎÅÄÈÍÅÍÈß Ñ ÁÈÇÍÅÑ-ËÎÃÈÊÎÉ ====================

    // Ñèãíàëû îò áèçíåñ-ëîãèêè ê GUI
    connect(m_businessLogic, &BusinessLogic::logMessage, this, &MainWindow::onLogMessage);
    connect(m_businessLogic, &BusinessLogic::connectionStateChanged, this, &MainWindow::onConnectionStateChanged);
    connect(m_businessLogic, &BusinessLogic::imageLoaded, this, &MainWindow::onImageLoaded);
    connect(m_businessLogic, &BusinessLogic::imageProcessed, this, &MainWindow::onImageProcessed);
    connect(m_businessLogic, &BusinessLogic::imageCleared, this, &MainWindow::onImageCleared);
    connect(m_businessLogic, &BusinessLogic::socketError, this, &MainWindow::onSocketError);
}

// ==================== ÐÅÀËÈÇÀÖÈß ÑËÎÒÎÂ ÄËß ÂÊËÀÄÊÈ "ÑÎÊÅÒ" ====================

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

// ==================== ÐÅÀËÈÇÀÖÈß ÑËÎÒÎÂ ÄËß ÂÊËÀÄÊÈ "ÐÀÇÂ¨ÐÒÊÀ" ====================

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
    ui->processImageButton->setEnabled(false);
    m_businessLogic->processImage();
}

void MainWindow::onClearImageClicked()
{
    m_businessLogic->clearImage();
}

// ==================== ÑËÎÒÛ ÄËß ÎÁÐÀÁÎÒÊÈ ÑÈÃÍÀËÎÂ ÎÒ ÁÈÇÍÅÑ-ËÎÃÈÊÈ ====================

void MainWindow::onLogMessage(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    updateSendButtonState();
}

void MainWindow::onImageLoaded(const QPixmap& originalImage, const QString& fileName)
{
    // 1. Ïîëó÷àåì ðàçìåðû imagePreviewLabel
    QSize labelSize = ui->imagePreviewLabel->size();

    // 2. Ïîëó÷àåì ðàçìåðû èñõîäíîãî èçîáðàæåíèÿ
    QSize imageSize = originalImage.size();

    // 3. Âû÷èñëÿåì ìàñøòàáèðîâàíèå ÷òîáû èçîáðàæåíèå âïèñûâàëîñü â label
    QPixmap scaledImage = originalImage.scaled(
        labelSize.width() - 10,    // -10 äëÿ íåáîëüøèõ îòñòóïîâ
        labelSize.height() - 10,
        Qt::KeepAspectRatio,       // Ñîõðàíÿåì ïðîïîðöèè
        Qt::SmoothTransformation   // Ïëàâíîå ìàñøòàáèðîâàíèå
    );

    // 4. Óñòàíàâëèâàåì ìàñøòàáèðîâàííîå èçîáðàæåíèå
    ui->imagePreviewLabel->setPixmap(scaledImage);
    ui->imagePathLabel->setText(fileName);

    // Îñòàëüíîé êîä áåç èçìåíåíèé
    updateImageButtonsState();
    ui->tabWidget->setCurrentIndex(1);
}

void MainWindow::onImageProcessed()
{
    ui->processImageButton->setEnabled(true);
    ui->tabWidget->setCurrentIndex(2);
}

void MainWindow::onImageCleared()
{
    ui->imagePreviewLabel->clear();
    ui->imagePreviewLabel->setText("Image Preview");
    ui->imagePathLabel->setText("No file selected");
    updateImageButtonsState();
}

void MainWindow::onSocketError(const QString& error)
{
    QMessageBox::warning(this, "Error", error);
}

// ==================== ÂÑÏÎÌÎÃÀÒÅËÜÍÛÅ ÌÅÒÎÄÛ ====================

void MainWindow::updateSendButtonState()
{
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = ui->disconnectButton->isEnabled();

    ui->sendMessageButton->setEnabled(hasText && isConnected);
}

void MainWindow::updateImageButtonsState()
{
    bool hasImage = !ui->imagePathLabel->text().isEmpty() && ui->imagePathLabel->text() != "No file selected";
    ui->processImageButton->setEnabled(hasImage);
    ui->clearImageButton->setEnabled(hasImage);
}