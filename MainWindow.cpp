#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "BusinessLogic.h"
#include "CropDialog.h"
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
    // Ïåðåìåùàåì áèçíåñ-ëîãèêó â îòäåëüíûé ïîòîê äëÿ ïðåäîòâðàùåíèÿ áëîêèðîâêè GUI
    m_businessLogic->moveToThread(m_businessThread);
    m_businessThread->start();

    // ==================== ÍÀÑÒÐÎÉÊÀ ÑÎÅÄÈÍÅÍÈÉ ÑÈÃÍÀËÎÂ È ÑËÎÒÎÂ ====================
    setupConnections();

    // ==================== ÈÍÈÖÈÀËÈÇÀÖÈß ÑÎÑÒÎßÍÈß ÈÍÒÅÐÔÅÉÑÀ ====================
    updateSendButtonState();
    updateImageButtonsState();
}

MainWindow::~MainWindow()
{
    // ==================== ÊÎÐÐÅÊÒÍÎÅ ÇÀÂÅÐØÅÍÈÅ ÏÎÒÎÊÀ ====================
    // Ïëàâíîå çàâåðøåíèå ðàáîòû ïîòîêà áèçíåñ-ëîãèêè
    m_businessThread->quit();
    m_businessThread->wait(1000);

    // Ïðèíóäèòåëüíîå çàâåðøåíèå åñëè ïîòîê íå îòâåòèë
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
    // Ñèãíàëû îò áèçíåñ-ëîãèêè ê GUI (ìåæïîòî÷íûå ñîåäèíåíèÿ)
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
    // Äèàëîã âûáîðà ôàéëà ñ ïîääåðæêîé ðàçëè÷íûõ ôîðìàòîâ
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
    // Äîáàâëåíèå âðåìåííîé ìåòêè ê êàæäîìó ñîîáùåíèþ â ëîãå
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    ui->logTextEdit->append(QString("[%1] %2").arg(timestamp, message));
}

void MainWindow::onConnectionStateChanged(bool connected)
{
    // Îáíîâëåíèå ñîñòîÿíèÿ êíîïîê ïîäêëþ÷åíèÿ/îòêëþ÷åíèÿ
    ui->connectButton->setEnabled(!connected);
    ui->disconnectButton->setEnabled(connected);
    updateSendButtonState();
}

void MainWindow::onImageLoaded(const QPixmap& originalImage, const QString& fileName)
{
    // Ïîêàçûâàåì äèàëîã êàäðèðîâàíèÿ ïåðåä îòîáðàæåíèåì èçîáðàæåíèÿ
    CropDialog cropDialog(this);
    cropDialog.setImage(originalImage);

    if (cropDialog.exec() == QDialog::Accepted) {
        // Ïîëó÷àåì êàäðèðîâàííîå èçîáðàæåíèå
        QPixmap croppedImage = cropDialog.getCroppedImage();

        // Ìàñøòàáèðóåì èçîáðàæåíèå äëÿ îòîáðàæåíèÿ â preview
        QSize labelSize = ui->imagePreviewLabel->size();
        QPixmap scaledImage = croppedImage.scaled(
            labelSize.width() - 10,
            labelSize.height() - 10,
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation
        );

        // Óñòàíàâëèâàåì èçîáðàæåíèå è èíôîðìàöèþ î ôàéëå
        ui->imagePreviewLabel->setPixmap(scaledImage);

        // Äîáàâëÿåì ïîìåòêó î êàäðèðîâàíèè åñëè îíî áûëî ïðèìåíåíî
        if (cropDialog.getCroppedImage().size() != originalImage.size()) {
            ui->imagePathLabel->setText(fileName + " (cropped)");
        }
        else {
            ui->imagePathLabel->setText(fileName);
        }

        updateImageButtonsState();
        ui->tabWidget->setCurrentIndex(1); // Ïåðåêëþ÷àåìñÿ íà âêëàäêó ñ èçîáðàæåíèåì
    }
}

void MainWindow::onImageProcessed()
{
    ui->processImageButton->setEnabled(true);
    ui->tabWidget->setCurrentIndex(2); // Ïåðåêëþ÷àåìñÿ íà âêëàäêó "Ðåçóëüòàòû"
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
    QMessageBox::warning(this, "Connection Error", error);
}

// ==================== ÂÑÏÎÌÎÃÀÒÅËÜÍÛÅ ÌÅÒÎÄÛ ====================
void MainWindow::updateSendButtonState()
{
    // Êíîïêà îòïðàâêè àêòèâíà òîëüêî ïðè íàëè÷èè òåêñòà è óñòàíîâëåííîì ñîåäèíåíèè
    bool hasText = !ui->messageLineEdit->text().trimmed().isEmpty();
    bool isConnected = ui->disconnectButton->isEnabled();
    ui->sendMessageButton->setEnabled(hasText && isConnected);
}

void MainWindow::updateImageButtonsState()
{
    // Êíîïêè îáðàáîòêè è î÷èñòêè àêòèâíû òîëüêî ïðè çàãðóæåííîì èçîáðàæåíèè
    bool hasImage = !ui->imagePathLabel->text().isEmpty() &&
        ui->imagePathLabel->text() != "No file selected";
    ui->processImageButton->setEnabled(hasImage);
    ui->clearImageButton->setEnabled(hasImage);
}