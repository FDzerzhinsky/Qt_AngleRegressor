// [file name]: MainWindow.h
#pragma once

#include <QMainWindow>
#include <QThread>
#include <QSettings>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMutex>
#include <QQueue>
#include <QTimer>

// Предварительное объявление классов
class BusinessLogic;
class CropDialog;
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // ==================== СЛОТЫ ДЛЯ ВКЛАДКИ "СОКЕТ" ====================
    void onConnectClicked();
    void onDisconnectClicked();
    void onClearLogClicked();
    void onSendMessageClicked();
    void onMessageTextChanged(const QString& text);

    // ==================== СЛОТЫ ДЛЯ ВКЛАДКИ "РАЗВЁРТКА" ====================
    void onLoadImageClicked();
    void onProcessImageClicked();
    void onClearImageClicked();

    // ==================== СЛОТЫ ДЛЯ ВКЛАДКИ "ВЫБОР РИСУНКА" ====================
    void onResultsTabActivated();
    void onSelectPatternClicked();
    void onPatternSelectionChanged();

    // ==================== СЛОТЫ ДЛЯ ОБРАБОТКИ СИГНАЛОВ ОТ БИЗНЕС-ЛОГИКИ ====================
    void onLogMessage(const QString& message);
    void onConnectionStateChanged(bool connected);
    void onImageLoaded(const QPixmap& preview, const QString& fileName);
    void onImageProcessed();
    void onImageCleared();
    void onSocketError(const QString& error);

    // ==================== СЛОТЫ ДЛЯ КОМПЬЮТЕРНОГО ЗРЕНИЯ ====================
    void onStartVisionClicked();
    void onStopVisionClicked();
    void onVisionResultReceived(const QString& snapshotName, int xPosition, double totalTime);
    void onVisionResultReceivedForDisplay(const QString& displayMessage);
    void onVisionSystemError(const QString& error);

    // ==================== НОВЫЕ СЛОТЫ ДЛЯ РАБОТЫ С СОКЕТОМ И СНЭПШОТАМИ ====================
    void onSaveSnapshotsToggled(bool checked);
    void onGetFromSocketToggled(bool checked);
    void onSocketDataReceived(const QString& data);

    // ==================== СЛОТ ДЛЯ АСИНХРОННОЙ ОБРАБОТКИ СОПОСТАВЛЕНИЯ ====================
    void onProcessDataPairing();

private:
    Ui::MainWindow* ui;

    // Бизнес-логика и её поток
    BusinessLogic* m_businessLogic;
    QThread* m_businessThread;

    // Диалог кадрирования и данные изображения
    CropDialog* m_cropDialog;
    QPixmap m_currentImage;
    QPixmap m_croppedImage;

    // Для работы с третьей вкладкой
    QString m_lastSavedImage;
    QSettings* m_settings;
    bool m_justSavedImage;

    // ==================== НОВЫЕ ПЕРЕМЕННЫЕ ДЛЯ СВЯЗИ СНЭПШОТОВ И СОКЕТА ====================
    QMutex m_dataMutex;
    bool m_socketConnected;
    QStringList m_pendingSocketValues;    // Список ожидающих значений из сокета
    QTimer* m_pairingTimer;               // Таймер для отложенной обработки

    // ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================
    void setupConnections();
    void updateSendButtonState();
    void updateImageButtonsState();
    QString getNextAvailableFilename();
    void updateNetNameEdit();
    void updateResultsList();
    void selectDefaultPattern();
    void initializeSettings();
    void selectFileInList(const QString& fileName);

    void updateGetFromSocketState();
    void processDataPairing();  // ОСНОВНОЙ МЕТОД ДЛЯ СОПОСТАВЛЕНИЯ ДАННЫХ
    void saveValuePair(const QString& snapshotName, const QString& socketValue);
    QString findLatestUnpairedSnapshot();
    QStringList findUnpairedSnapshots();
    QSet<QString> readPairedSnapshotsFromValues();  // чтение сопоставленных снэпшотов
    int getSnapshotCount();
    int countValuePairs();
};