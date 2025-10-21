#pragma once

#include <QMainWindow>
#include <QTcpSocket>
#include <QTimer>
#include <QDateTime>
#include <QPixmap>
#include <QFileDialog>
#include <QMessageBox>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

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
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);
    void onSocketReadyRead();
    void onClearLogClicked();
    void onSendMessageClicked();
    void onMessageTextChanged(const QString& text);

    // ==================== СЛОТЫ ДЛЯ ВКЛАДКИ "РАЗВЁРТКА" ====================
    void onLoadImageClicked();
    void onProcessImageClicked();
    void onClearImageClicked();

private:
    Ui::MainWindow* ui;
    QTcpSocket* tcpSocket;

    // Переменные для работы с изображениями
    QPixmap currentImage;
    QString currentImagePath;

    // Вспомогательные методы
    void logMessage(const QString& message);
    void updateSendButtonState();
    void updateImageButtonsState();
};