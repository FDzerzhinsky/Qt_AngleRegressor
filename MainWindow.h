#pragma once

#include <QMainWindow>
#include <QThread>
#include <QSettings>
#include <QPushButton>      // Добавляем
#include <QLabel>           // Добавляем  
#include <QTextEdit>        // Добавляем
#include <QVBoxLayout>      // Добавляем
#include <QHBoxLayout>      // Добавляем
#include <QMessageBox>      // Добавляем

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
    void onVisionSystemError(const QString& error);

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
    bool m_justSavedImage;  // Флаг, указывающий что мы только что сохранили изображение

    // ==================== КОМПЬЮТЕРНОЕ ЗРЕНИЕ ====================
    QPushButton* m_startVisionButton;
    QPushButton* m_stopVisionButton;
    QLabel* m_visionStatusLabel;
    QTextEdit* m_visionResultsTextEdit;

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
    void setupVisionTab();  // Добавляем объявление
};