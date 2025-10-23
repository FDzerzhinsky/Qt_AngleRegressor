#pragma once

#include <QMainWindow>
#include <QThread>

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

    // ==================== СЛОТЫ ДЛЯ ОБРАБОТКИ СИГНАЛОВ ОТ БИЗНЕС-ЛОГИКИ ====================
    void onLogMessage(const QString& message);
    void onConnectionStateChanged(bool connected);
    void onImageLoaded(const QPixmap& preview, const QString& fileName);
    void onImageProcessed();
    void onImageCleared();
    void onSocketError(const QString& error);
    
private:
    Ui::MainWindow* ui;

    // Бизнес-логика и её поток
    BusinessLogic* m_businessLogic;
    QThread* m_businessThread;

    // Диалог кадрирования и данные изображения
    CropDialog* m_cropDialog;
    QPixmap m_currentImage;
    QPixmap m_croppedImage;

    // ==================== ВСПОМОГАТЕЛЬНЫЕ МЕТОДЫ ====================
    void setupConnections();
    void updateSendButtonState();
    void updateImageButtonsState();
    QString getNextAvailableFilename();
    void updateNetNameEdit();
};