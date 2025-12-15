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

    // ==================== СЛОТЫ ДЛЯ ВКЛАДКИ "ВЫБОР МОДЕЛИ" ====================
    void onResultsTabActivated();
    void onSelectPatternClicked();
    void onPatternSelectionChanged();

    // ==================== СЛОТЫ ДЛЯ ОБРАБОТКИ СИГНАЛОВ ОТ БИЗНЕС-ЛОГИКИ ====================
    void onLogMessage(const QString& message);
    void onConnectionStateChanged(bool connected);
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

    // Для работы с сопоставлениями и очередь сокета
    QSettings* m_settings;
    QMutex m_dataMutex;
    bool m_socketConnected;
    QStringList m_pendingSocketValues;    // Список ожидающих значений из сокета
    QTimer* m_pairingTimer;               // Таймер для отложенной обработки

    // ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================
    void setupConnections();
    void updateSendButtonState();
    void updateResultsList();
    void selectDefaultPattern();
    void initializeSettings();
    void selectFileInList(const QString& fileName);

    void processDataPairing();  // ОСНОВНОЙ МЕТОД ДЛЯ СОПОСТАВЛЕНИЯ ДАННЫХ
    void saveValuePair(const QString& snapshotName, const QString& socketValue);
    QString findLatestUnpairedSnapshot();
    QStringList findUnpairedSnapshots();
    QSet<QString> readPairedSnapshotsFromValues();  // чтение сопоставленных снэпшотов
    int getSnapshotCount();
    int countValuePairs();
};